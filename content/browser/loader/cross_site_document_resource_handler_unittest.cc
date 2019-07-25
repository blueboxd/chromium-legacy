// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Warning: This file is slated for removal. Add new tests to
// cross_origin_read_blocking_unittests.cc instead.

// TODO(lukasza): These tests have been copied to
// cross_origin_read_blocking_unittests.cc. We should delete this file once the
// document resource handler path has been fully removed from Chrome.

#include "content/browser/loader/cross_site_document_resource_handler.h"

#include <stdint.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/loader/intercepting_resource_handler.h"
#include "content/browser/loader/mime_sniffing_resource_handler.h"
#include "content/browser/loader/mock_resource_loader.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/test_resource_handler.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_plugin_service.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using MimeType = network::CrossOriginReadBlocking::MimeType;
using MimeTypeBucket =
    network::CrossOriginReadBlocking::ResponseAnalyzer::MimeTypeBucket;
using CrossOriginProtectionDecision = network::CrossOriginReadBlocking::
    ResponseAnalyzer::CrossOriginProtectionDecision;

namespace content {

namespace {

enum class OriginHeader { kOmit, kInclude };

enum class Verdict {
  kAllow,
  kBlock,
};

constexpr int kVerdictPacketForHeadersBasedVerdict = -1;

// This struct is used to describe each test case in this file.  It's passed as
// a test parameter to each TEST_P test.
struct TestScenario {
  // Attributes to make test failure messages useful.
  const char* description;
  int source_line;

  // Attributes of the HTTP Request.
  const char* target_url;
  ResourceType resource_type;
  const char* initiator_origin;
  OriginHeader cors_request;

  // Attributes of the HTTP response.
  const char* response_headers;
  const char* response_content_type;
  MimeType canonical_mime_type;
  // Categorizes the MIME type as public, (CORB) protected or other.
  MimeTypeBucket mime_type_bucket;
  // |packets| specifies the response data which may arrive over the course of
  // several writes.
  std::initializer_list<const char*> packets;

  std::string data() const {
    std::string data;
    for (const char* packet : packets) {
      data += packet;
    }
    return data;
  }

  // Whether the resource should seem sensitive (either through the CORS
  // heuristic or the Cache heuristic). This is used for testing that CORB would
  // have protected the resource, were it requested cross-origin.
  bool resource_is_sensitive;
  // Whether we expect CORB to protect the resource on a cross-origin request.
  // Note this value is not checked if resource_is_sensitive is false. Also note
  // that the protection decision may be
  // kBlockedAfterSniffing/kAllowedAfterSniffing despite a nosniff header as we
  // still sniff for the javascript parser breaker in these cases.
  CrossOriginProtectionDecision protection_decision;

  // Expected result.
  Verdict verdict;
  // The packet number during which the verdict is decided.
  // kVerdictPacketForHeadersBasedVerdict means that the verdict can be decided
  // before the first packet's data is available. |packets.size()| means that
  // the verdict is decided during the end-of-stream call.
  int verdict_packet;
};

// Stream operator to let GetParam() print a useful result if any tests fail.
::std::ostream& operator<<(::std::ostream& os, const TestScenario& scenario) {

  std::string verdict;
  switch (scenario.verdict) {
    case Verdict::kAllow:
      verdict = "Verdict::kAllow";
      break;
    case Verdict::kBlock:
      verdict = "Verdict::kBlock";
      break;
  }

  std::string response_headers_formatted;
  base::ReplaceChars(scenario.response_headers, "\n",
                     "\n                          ",
                     &response_headers_formatted);

  std::string mime_type_bucket;
  switch (scenario.mime_type_bucket) {
    case MimeTypeBucket::kProtected:
      mime_type_bucket = "MimeTypeBucket::kProtected";
      break;
    case MimeTypeBucket::kPublic:
      mime_type_bucket = "MimeTypeBucket::kPublic";
      break;
    case MimeTypeBucket::kOther:
      mime_type_bucket = "MimeTypeBucket::kOther";
      break;
  }

  std::string packets = "{";
  for (std::string packet : scenario.packets) {
    base::ReplaceChars(packet, "\\", "\\\\", &packet);
    base::ReplaceChars(packet, "\"", "\\\"", &packet);
    base::ReplaceChars(packet, "\n", "\\n", &packet);
    base::ReplaceChars(packet, "\t", "\\t", &packet);
    base::ReplaceChars(packet, "\r", "\\r", &packet);
    if (packets.length() > 1)
      packets += ", ";
    packets += "\"";
    packets += packet;
    packets += "\"";
  }
  packets += "}";

  std::string protection_decision;
  switch (scenario.protection_decision) {
    case CrossOriginProtectionDecision::kAllow:
      protection_decision = "CrossOriginProtectionDecision::kAllow";
      break;
    case CrossOriginProtectionDecision::kBlock:
      protection_decision = "CrossOriginProtectionDecision::kBlock";
      break;
    case CrossOriginProtectionDecision::kNeedToSniffMore:
      protection_decision = "CrossOriginProtectionDecision::kNeedToSniffMore";
      break;
    case CrossOriginProtectionDecision::kAllowedAfterSniffing:
      protection_decision =
          "CrossOriginProtectionDecision::kAllowedAfterSniffing";
      break;
    case CrossOriginProtectionDecision::kBlockedAfterSniffing:
      protection_decision =
          "CrossOriginProtectionDecision::kBlockedAfterSniffing";
      break;
  }

  return os << "\n  description           = " << scenario.description
            << "\n  target_url            = " << scenario.target_url
            << "\n  resource_type         = "
            << static_cast<int>(scenario.resource_type)
            << "\n  initiator_origin      = " << scenario.initiator_origin
            << "\n  cors_request          = "
            << (scenario.cors_request == OriginHeader::kOmit
                    ? "OriginHeader::kOmit"
                    : "OriginHeader::kInclude")
            << "\n  response_headers      = " << response_headers_formatted
            << "\n  response_content_type = " << scenario.response_content_type
            << "\n  canonical_mime_type   = " << scenario.canonical_mime_type
            << "\n  mime_type_bucket      = " << mime_type_bucket
            << "\n  packets               = " << packets
            << "\n  resource_is_sensitive = "
            << (scenario.resource_is_sensitive ? "true" : "false")
            << "\n  protection_decision   = " << protection_decision
            << "\n  verdict               = " << verdict
            << "\n  verdict_packet        = " << scenario.verdict_packet;
}

// An HTML response with an HTML comment that's longer than the sniffing
// threshhold. We don't sniff past net::kMaxBytesToSniff, so these are not
// protected
const char kHTMLWithTooLongComment[] =
    "<!--.............................................................72 chars"
    "................................................................144 chars"
    "................................................................216 chars"
    "................................................................288 chars"
    "................................................................360 chars"
    "................................................................432 chars"
    "................................................................504 chars"
    "................................................................576 chars"
    "................................................................648 chars"
    "................................................................720 chars"
    "................................................................792 chars"
    "................................................................864 chars"
    "................................................................936 chars"
    "...............................................................1008 chars"
    "...............................................................1080 chars"
    "--><html><head>";

// A set of test cases that verify CrossSiteDocumentResourceHandler correctly
// classifies network responses as allowed or blocked.  These TestScenarios are
// passed to the TEST_P tests below as test parameters.
const TestScenario kScenarios[] = {

    // Allowed responses (without sniffing):
    {
        "Allowed: Same-site XHR to HTML",
        __LINE__,
        "http://www.a.com/resource.html",           // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Same-origin JSON with parser breaker and HTML mime type",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {")]}',\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Same-origin JSON with parser breaker and JSON mime type",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {")]}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Cross-site script without parser breaker",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "application/javascript",          // response_content_type
        MimeType::kOthers,                 // canonical_mime_type
        MimeTypeBucket::kPublic,           // mime_type_bucket
        {"var x=3;"},                      // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to HTML with CORS for origin",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        // Normally verdict_packet = kVerdictPacketForHeadersBasedVerdict.
        0,  // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to XML with CORS for any",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: *",  // response_headers
        "application/rss+xml",             // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to JSON with CORS for null",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: null",    // response_headers
        "text/json",                            // response_content_type
        MimeType::kJson,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"{\"x\" : 3}"},                        // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to HTML over FTP",
        __LINE__,
        "ftp://www.b.com/resource.html",            // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to HTML from file://",
        __LINE__,
        "file:///foo/resource.html",                // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        // Blocked, because the unit test doesn't make a call to
        // CrossOriginReadBlocking::AddExceptionForFlash (simulating a behavior
        // of a compromised renderer that only pretends to be hosting Flash).
        //
        // The regular scenario is covered by integration tests:
        // OutOfProcessPPAPITest.URLLoaderTrusted.
        "Blocked: Cross-site fetch HTML from Flash without CORS",
        __LINE__,
        "http://www.b.com/plugin.html",  // target_url
        ResourceType::kPluginResource,   // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: Cross-site fetch HTML from NaCl with CORS response",
        __LINE__,
        "http://www.b.com/plugin.html",  // target_url
        ResourceType::kPluginResource,   // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kInclude,          // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // first_chunk
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: JSON object + CORS with parser-breaker labeled as JavaScript",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: *",       // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {")]}'\n[true, false]"},                // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: JSON object labeled as JavaScript with a no-sniff header",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"{ \"key\"", ": true }"},              // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        1,                                      // verdict_packet
    },
    {
        "Allowed: Empty response with PNG mime type",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "image/png",                       // response_content_type
        MimeType::kOthers,                 // canonical_mime_type
        MimeTypeBucket::kPublic,           // mime_type_bucket
        {},                                // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Empty response with PNG mime type and nosniff header",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "image/png",                        // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {},                                 // packets
        false,                              // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },

    // Allowed responses due to sniffing:
    {
        "Allowed: Cross-site script to JSONP labeled as HTML",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"foo({\"x\" : 3})"},              // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site script to JavaScript labeled as text",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"var x = 3;"},                    // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: JSON-like JavaScript labeled as text",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"{", "    \n", "var x = 3;\n", "console.log('hello');"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        2,                          // verdict_packet
    },

    {
        "Allowed: JSONP labeled as JSON",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"invoke({ \"key\": true });"},    // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed (for now): JSON array literal labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",      // target_url
        ResourceType::kScript,                 // resource_type
        "http://www.a.com/",                   // initiator_origin
        OriginHeader::kOmit,                   // cors_request
        "HTTP/1.1 200 OK",                     // response_headers
        "text/plain",                          // response_content_type
        MimeType::kPlain,                      // canonical_mime_type
        MimeTypeBucket::kProtected,            // mime_type_bucket
        {"[1, 2, {}, true, false, \"yay\"]"},  // packets
        false,                                 // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: JSON array literal on which a function is called.",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"[1, 2, {}, true, false, \"yay\"]", ".map(x => console.log(x))",
         ".map(x => console.log(x));"},  // packets
        false,                           // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to nonsense labeled as XML",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "application/xml",                 // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"Won't sniff as XML"},            // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to nonsense labeled as JSON",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"Won't sniff as JSON"},           // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to partial match for <HTML> tag",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<htm"},                          // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        1,                          // verdict_packet
    },
    {
        "Allowed: HTML tag appears only after net::kMaxBytesToSniff",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {kHTMLWithTooLongComment},         // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Empty response with html mime type",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {},                                // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Same-site XHR to a filesystem URI",
        __LINE__,
        "filesystem:http://www.a.com/file.html",    // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Same-site XHR to a blob URI",
        __LINE__,
        "blob:http://www.a.com/guid-goes-here",     // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },

    // Blocked responses (without sniffing):
    {
        "Blocked: Cross-site XHR to nosniff HTML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: nosniff + Content-Type: text/html; charset=utf-8",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html; charset=utf-8",                 // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to nosniff response without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "text/html",                            // response_content_type
        MimeType::kHtml,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"Wouldn't sniff as HTML"},             // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: Cross-origin, same-site XHR to nosniff HTML without CORS",
        __LINE__,
        "https://foo.site.com/resource.html",  // target_url
        ResourceType::kXhr,                    // resource_type
        "https://bar.site.com/",               // initiator_origin
        OriginHeader::kOmit,                   // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker/html/nosniff",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        ResourceType::kXhr,            // resource_type
        "http://c.com/",               // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "text/html",                        // response_content_type
        MimeType::kHtml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,         // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    {
        // This scenario is unusual, since there's no difference between
        // a blocked response and a non-blocked response; the CSDRH doesn't
        // actually have a chance to cancel the connection.
        "Blocked(-ish?): Nosniff header + empty response",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "text/html",                            // response_content_type
        MimeType::kHtml,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {},                                     // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    // Blocked responses due to sniffing:
    {
        "Blocked: Cross-origin XHR to HTML with wrong CORS (okay same-site)",
        // Note that initiator_origin is cross-origin, but same-site in relation
        // to the CORS response.
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://foo.example.com/",         // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to HTML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",           // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to XML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "application/xml",                 // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to JSON without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "application/json",                // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"{\"x\" : 3}"},                   // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving JSON labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",             // target_url
        ResourceType::kXhr,                           // resource_type
        "http://www.a.com/",                          // initiator_origin
        OriginHeader::kOmit,                          // cors_request
        "HTTP/1.1 200 OK",                            // response_headers
        "text/plain",                                 // response_content_type
        MimeType::kPlain,                             // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        false,                                        // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        5,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving xml labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",              // target_url
        ResourceType::kXhr,                            // resource_type
        "http://www.a.com/",                           // initiator_origin
        OriginHeader::kOmit,                           // cors_request
        "HTTP/1.1 200 OK",                             // response_headers
        "text/plain",                                  // response_content_type
        MimeType::kPlain,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                    // mime_type_bucket
        {"    ", "\t", "<", "?", "x", "m", "l", ">"},  // packets
        false,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        6,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving html labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"    <!--", "\t -", "-", "->", "\n", "<", "s", "c", "r", "i", "p",
         "t"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        11,                         // verdict_packet
    },
    {
        "Blocked: slow-arriving html with commented-out xml tag",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"    <!--", " <?xml ", "-->\n", "<", "h", "e", "a", "d"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        7,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to HTML labeled as text without CORS",
        __LINE__,
        "http://www.b.com/resource.html",           // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/plain",                               // response_content_type
        MimeType::kPlain,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site <script> inclusion of HTML w/ DTD without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<!doc", "type html><html itemscope=\"\" ",
         "itemtype=\"http://schema.org/SearchResultsPage\" ",
         "lang=\"en\"><head>"},  // packets
        false,                   // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to HTML with wrong CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site fetch HTML from NaCl without CORS response",
        __LINE__,
        "http://www.b.com/plugin.html",             // target_url
        ResourceType::kPluginResource,              // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kInclude,                     // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // first_chunk
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker and JSON mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        ResourceType::kXhr,            // resource_type
        "http://c.com/",               // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 200 OK",             // response_headers
        "text/json",                   // response_content_type
        MimeType::kJson,               // canonical_mime_type
        MimeTypeBucket::kProtected,    // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker/nosniff/other mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        ResourceType::kXhr,            // resource_type
        "http://c.com/",               // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "audio/x-wav",                      // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker and other mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        ResourceType::kXhr,            // resource_type
        "http://c.com/",               // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 200 OK",             // response_headers
        "application/javascript",      // response_content_type
        MimeType::kOthers,             // canonical_mime_type
        MimeTypeBucket::kPublic,       // mime_type_bucket
        {"for(;;)", ";[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: JSON object + mismatching CORS with parser-breaker labeled "
        "as JavaScript",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "application/javascript",           // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {")]}'\n[true, false]"},            // packets
        true,                               // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to a filesystem URI",
        __LINE__,
        "filesystem:http://www.b.com/file.html",    // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to a blob URI",
        __LINE__,
        "blob:http://www.b.com/guid-goes-here",     // target_url
        ResourceType::kXhr,                         // resource_type
        "http://www.a.com/",                        // initiator_origin
        OriginHeader::kOmit,                        // cors_request
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    // Range response.  The product code doesn't currently look at the exact
    // range specified, so we can get away with testing with arbitrary/random
    // values.
    {
        "Allowed: Javascript 206",
        __LINE__,
        "http://www.b.com/script.js",  // target_url
        ResourceType::kScript,         // resource_type
        "http://www.a.com/",           // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"x = 1;"},                             // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        -1,                                     // verdict_packet
    },
    {
        // Here the resources is allowed cross-origin from b.com to a.com
        // because of the CORS header. However CORB still blocks c.com from
        // accessing it (so the protection decision should be kBlock).
        "Allowed: text/html 206 media with CORS",
        __LINE__,
        "http://www.b.com/movie.html",  // target_url
        ResourceType::kMedia,           // resource_type
        "http://www.a.com/",            // initiator_origin
        OriginHeader::kInclude,         // cors_request
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                             // response_content_type
        MimeType::kHtml,                         // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"simulated *middle*-of-html content"},  // packets
        true,                                    // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,   // protection_decision
        Verdict::kAllow,                         // verdict
        -1,                                      // verdict_packet
    },
    {
        "Allowed: text/plain 206 media",
        __LINE__,
        "http://www.b.com/movie.txt",  // target_url
        ResourceType::kMedia,          // resource_type
        "http://www.a.com/",           // initiator_origin
        OriginHeader::kOmit,           // cors_request
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "text/plain",                           // response_content_type
        MimeType::kPlain,                       // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"movie content"},                      // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        -1,                                     // verdict_packet
    },
    {
        "Blocked: text/html 206 media",
        __LINE__,
        "http://www.b.com/movie.html",  // target_url
        ResourceType::kMedia,           // resource_type
        "http://www.a.com/",            // initiator_origin
        OriginHeader::kOmit,            // cors_request
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",   // response_headers
        "text/html",                             // response_content_type
        MimeType::kHtml,                         // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"simulated *middle*-of-html content"},  // packets
        false,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,   // protection_decision
        Verdict::kBlock,                         // verdict
        -1,                                      // verdict_packet
    },
    // Responses with no data.
    {
        "Allowed: same-origin 204 response with no data",
        __LINE__,
        "http://a.com/resource.html",              // target_url
        ResourceType::kXhr,                        // resource_type
        "http://a.com/",                           // initiator_origin
        OriginHeader::kOmit,                       // cors_request
        "HTTP/1.1 204 NO CONTENT",                 // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed after sniffing: cross-origin 204 response with no data",
        __LINE__,
        "http://a.com/resource.html",              // target_url
        ResourceType::kXhr,                        // resource_type
        "http://b.com/",                           // initiator_origin
        OriginHeader::kOmit,                       // cors_request
        "HTTP/1.1 204 NO CONTENT",                 // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },

    // Testing the CORB protection logging.
    {
        "Not Sensitive: script without CORS or Cache heuristic",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin",                         // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Not Sensitive: vary user-agent is present and should be ignored",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin, User-Agent",             // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Not Sensitive: cache-control no-store should be ignored",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: No-Store",              // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    // Responses with the Access-Control-Allow-Origin header value other than *.
    {
        "Sensitive, Allowed: script with CORS heuristic and range header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kInclude,          // cors_request
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Blocked: html with CORS heuristic and no sniff",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Sensitive, Blocked after sniffing: html with CORS heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive, Allowed after sniffing: javascript with CORS heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "application/javascript",  // response_content_type
        MimeType::kOthers,         // canonical_mime_type
        MimeTypeBucket::kPublic,   // mime_type_bucket
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive slow-arriving JSON with CORS heurisitic. Only needs "
        "sniffing for the CORP protection statistics.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                  // response_content_type
        MimeType::kHtml,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        5,                          // verdict_packet
    },

    // Responses with Vary: Origin and Cache-Control: Private headers.
    {
        "Sensitive, Allowed: script with cache heuristic and range header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Allowed: script with cache heuristic and range "
        "header. Has vary user agent + cache no store which should not "
        "confuse the cache heuristic.",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kOmit,             // cors_request
        "HTTP/1.1 206 OK\n"
        "Vary: Origin, User-Agent\n"
        "Cache-Control: Private, No-Store\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Blocked: html with cache heuristic and no sniff",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Sensitive, Blocked after sniffing: html with cache heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive, Allowed after sniffing: javascript with cache heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kScript,             // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",  // response_headers
        "application/javascript",  // response_content_type
        MimeType::kOthers,         // canonical_mime_type
        MimeTypeBucket::kPublic,   // mime_type_bucket
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive slow-arriving JSON with cache heurisitic. Only needs "
        "sniffing for the CORP protection statistics.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                     // response_headers
        "text/html",                                  // response_content_type
        MimeType::kHtml,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        5,                          // verdict_packet
    },

    // The next two tests together ensure that when CORB blocks and strips the
    // cache/vary headers from a sensitive response, the CORB protection logging
    // still correctly identifies the response as sensitive and reports it.
    //
    // Here the protection logging reports immediately (without sniffing). So we
    // can (somewhat) safely assume the response is being correctly reported as
    // sensitive. Thus this test ensures the testing infrastructure itself is
    // also correctly idenitfying the response as sensitive.
    {
        "Sensitive cache heuristic, both CORB and the protection stats block",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.b.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    // Here the protection logging only reports after sniffing. Despite this,
    // the resource should still be identified as sensitive.
    {
        "Sensitive cache heuristic, both CORB and the protection stats block",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.b.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },

    // A test that makes sure we don't double log the CORB protection stats.
    // Because the request is cross-origin + same-site and has a same-site CORP
    // header, CORB needs to sniff. However CORB protection logging makes the
    // request cross-site and so needs no sniffing. We don't want the protection
    // logging to be triggered a second time after the sniffing.
    {
        "Sensitive, CORB needs sniffing (so verdict_packet > -1) but the CORB "
        "protection stats block based on headers",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.foo.a.com/",           // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Cross-Origin-Resource-Policy: same-site\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        0,                                          // verdict_packet
    },

    // Response with an unknown MIME type.
    {
        "Sensitive, Allowed: unknown MIME type with CORS heuristic and range "
        "header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kInclude,          // cors_request
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "unknown/mime_type",                    // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kOther,                 // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    // Responses with the accept-ranges header.
    {
        "Sensitive response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive response with an accept-ranges header but value |none|.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: none\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Non-sensitive response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes",                     // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    // Sensitive responses with the accept-ranges header, a protected MIME type
    // and protection decision = kBlock.
    {
        "CORS-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Cache-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kOmit,               // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Cache-Control: private\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Cache + CORS heuristics, accept-ranges header says |none|.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        ResourceType::kXhr,                // resource_type
        "http://www.a.com/",               // initiator_origin
        OriginHeader::kInclude,            // cors_request
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: none\n"
        "Cache-Control: private\n"
        "Access-Control-Allow-Origin: http://www.a.com/\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    // A Sensitive response with the accept-ranges header and a protected MIME
    // type but protection decision = kAllow (so the secondary accept-ranges
    // stats should not be reported).
    {
        "Sensitive + accept-ranges header but protection decision = kAllow.",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        ResourceType::kScript,           // resource_type
        "http://www.a.com/",             // initiator_origin
        OriginHeader::kInclude,          // cors_request
        "HTTP/1.1 206 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    // Sensitive responses with no data.
    {
        "Sensitive, allowed after sniffing: same-origin 204 with no data",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        ResourceType::kXhr,            // resource_type
        "http://a.com/",               // initiator_origin
        OriginHeader::kInclude,        // cors_request
        "HTTP/1.1 204 NO CONTENT\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        true,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
};

// TestResourceDispatcherHost is a ResourceDispatcherHostImpl that the test
// passes to MimeSniffingResourceHandler.  Since MimeSniffingResourceHandler
// only calls 2 methods of ResourceDispatcherHostImpl, only these 2 methods have
// been overriden below (we expect no calls - this is why the bodies only have
// the NOTREACHED macro in them).  This pattern of overriding only 2 methods has
// been copied from mime_sniffing_resource_handler_unittest.cc
class TestResourceDispatcherHost : public ResourceDispatcherHostImpl {
 public:
  TestResourceDispatcherHost() {}

  std::unique_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request) override {
    NOTREACHED();
    return std::make_unique<TestResourceHandler>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHost);
};

}  // namespace

// Tests that verify CrossSiteDocumentResourceHandler correctly classifies
// network responses as allowed or blocked, and ensures that empty responses are
// sent for the blocked cases.
//
// The various test cases are passed as a list of TestScenario structs.
class CrossSiteDocumentResourceHandlerTest
    : public testing::Test,
      public testing::WithParamInterface<TestScenario> {
 public:
  CrossSiteDocumentResourceHandlerTest()
      : stream_sink_status_(
            net::URLRequestStatus::FromError(net::ERR_IO_PENDING)) {
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }

  // Sets up the request, and the chain of 1) MockResourceLoader, 2)
  // CrossSiteDocumentResourceHandler, 3) TestResourceHandler.  If
  // |inject_mime_sniffer| is specified then a MimeSniffingResourceHandler will
  // be injected between MockResourceLoader and
  // CrossSiteDocumentResourceHandler.
  void Initialize(const std::string& target_url,
                  ResourceType resource_type,
                  const std::string& initiator_origin,
                  OriginHeader cors_request,
                  bool inject_mime_sniffer) {
    stream_sink_status_ = net::URLRequestStatus::FromError(net::ERR_IO_PENDING);

    // Initialize |request_| from the parameters.
    request_ = context_.CreateRequest(GURL(target_url), net::DEFAULT_PRIORITY,
                                      &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    ResourceRequestInfo::AllocateForTesting(
        request_.get(), resource_type,
        nullptr,                              // context
        3,                                    // render_process_id
        2,                                    // render_view_id
        1,                                    // render_frame_id
        true,                                 // is_main_frame
        ResourceInterceptPolicy::kAllowNone,  // resource_intercept_policy
        true,                                 // is_async
        PREVIEWS_OFF,                         // previews_state
        nullptr);                             // navigation_ui_data
    request_->set_initiator(url::Origin::Create(GURL(initiator_origin)));

    // Create a sink handler to capture results.
    auto stream_sink = std::make_unique<TestResourceHandler>(
        &stream_sink_status_, &stream_sink_body_);
    stream_sink_ = stream_sink->GetWeakPtr();

    // Create the CrossSiteDocumentResourceHandler.
    auto request_mode = cors_request == OriginHeader::kOmit
                            ? network::mojom::RequestMode::kNoCors
                            : network::mojom::RequestMode::kCors;
    auto document_blocker = std::make_unique<CrossSiteDocumentResourceHandler>(
        std::move(stream_sink), request_.get(), request_mode);
    document_blocker_ = document_blocker.get();
    first_handler_ = std::move(document_blocker);

    // Inject MimeSniffingResourceHandler if requested.
    if (inject_mime_sniffer) {
      intercepting_handler_ = std::make_unique<InterceptingResourceHandler>(
          std::make_unique<TestResourceHandler>(), nullptr);
      first_handler_ = std::make_unique<MimeSniffingResourceHandler>(
          std::move(first_handler_), &dispatcher_host_, &plugin_service_,
          intercepting_handler_.get(), request_.get(),
          blink::mojom::RequestContextType::SCRIPT);
    }

    // Create a mock loader to drive our chain of resource loaders.
    mock_loader_ = std::make_unique<MockResourceLoader>(first_handler_.get());
  }

  // Returns a ResourceResponse that matches the TestScenario's parameters.
  scoped_refptr<network::ResourceResponse> CreateResponse(
      const char* response_content_type,
      const char* raw_response_headers,
      const char* initiator_origin) {
    scoped_refptr<network::ResourceResponse> response =
        base::MakeRefCounted<network::ResourceResponse>();
    std::string formatted_response_headers =
        net::HttpUtil::AssembleRawHeaders(raw_response_headers);
    scoped_refptr<net::HttpResponseHeaders> response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            formatted_response_headers);

    // Content-Type header.
    std::string charset;
    bool had_charset = false;
    response_headers->AddHeader(std::string("Content-Type: ") +
                                response_content_type);
    net::HttpUtil::ParseContentType(response_content_type,
                                    &response->head.mime_type, &charset,
                                    &had_charset, nullptr);
    EXPECT_FALSE(response->head.mime_type.empty())
        << "Invalid MIME type defined in kScenarios.";
    response->head.headers = response_headers;

    return response;
  }

  // Determines the number of times that OnWillRead should be called on
  // |stream_sink_| at the point in the test indicated by |current_packet|.
  int GetExpectedNumberOfOnWillReadCalls(int current_packet) {
    TestScenario scenario = GetParam();

    // In all block scenarios, OnWillRead is called only once.
    if (scenario.verdict == Verdict::kBlock)
      return 1;

    // If we're not yet at the point of the verdict packet, OnWillRead has only
    // been called only once, since the resource handler doesn't yet know if
    // we're blocking or allowing yet.
    if (current_packet < scenario.verdict_packet)
      return 1;

    // If we are streaming from the start, we'll call |stream_sink_|'s
    // OnWillRead every time.
    if (scenario.verdict_packet <= 0)
      return current_packet + 1;

    // When the verdict is decided on the last packet (and the response was not
    // empty), OnWillRead should be called exactly twice.
    if (scenario.verdict_packet == static_cast<int>(scenario.packets.size()))
      return 2;

    // Otherwise, we buffer up until the verdict_packet, and then stream
    // thereafter.
    return 1 + (current_packet - scenario.verdict_packet);
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequest> request_;

  // |stream_sink_| is the handler that's immediately after |document_blocker_|
  // in the ResourceHandler chain; it records the values passed to it into
  // |stream_sink_status_| and |stream_sink_body_|, which our tests assert
  // against.
  //
  // |stream_sink_| is owned by |document_blocker_|, but we retain a reference
  // to it.
  base::WeakPtr<TestResourceHandler> stream_sink_;
  net::URLRequestStatus stream_sink_status_;
  std::string stream_sink_body_;

  // |document_blocker_| is the CrossSiteDocuemntResourceHandler instance under
  // test.
  CrossSiteDocumentResourceHandler* document_blocker_ = nullptr;

  // |first_handler_| is the first resource handler in a chain or resource
  // handlers that eventually reached CrossSiteDocumentResourceHandler.
  std::unique_ptr<LayeredResourceHandler> first_handler_;

  // |mock_loader_| is the mock loader used to drive |document_blocker_|.
  std::unique_ptr<MockResourceLoader> mock_loader_;

  FakePluginService plugin_service_;
  TestResourceDispatcherHost dispatcher_host_;
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentResourceHandlerTest);
};

// Runs a particular TestScenario (passed as the test's parameter) through the
// ResourceLoader and CrossSiteDocumentResourceHandler, verifying that the
// response is correctly allowed or blocked based on the scenario.
TEST_P(CrossSiteDocumentResourceHandlerTest, ResponseBlocking) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  Initialize(scenario.target_url, scenario.resource_type,
             scenario.initiator_origin, scenario.cors_request,
             false /* = inject_mime_sniffer */);
  base::HistogramTester histograms;

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  // Set up response based on scenario.
  scoped_refptr<network::ResourceResponse> response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));

  // Verify MIME type was classified correctly.
  ASSERT_TRUE(document_blocker_->has_response_started_);
  EXPECT_EQ(scenario.canonical_mime_type,
            document_blocker_->analyzer_->canonical_mime_type_for_testing());

  // Verify that we will sniff content into a different buffer if sniffing is
  // needed.  Note that the different buffer is used even for blocking cases
  // where no sniffing is needed, to avoid complexity in the handler.  The
  // handler doesn't look at the data in that case, but there's no way to verify
  // it in the test.
  bool expected_to_sniff =
      scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict;
  ASSERT_EQ(expected_to_sniff, document_blocker_->analyzer_->needs_sniffing());

  // Verify that we correctly decide whether to block based on headers.  Note
  // that this includes cases that will later be allowed after sniffing.
  bool expected_to_block_based_on_headers =
      expected_to_sniff || scenario.verdict == Verdict::kBlock;
  ASSERT_EQ(expected_to_block_based_on_headers,
            document_blocker_->should_block_based_on_headers_);

  // This vector holds the packets to be delivered.
  std::vector<const char*> packets_vector(scenario.packets);
  packets_vector.push_back("");  // End-of-stream is marked by an empty packet.

  bool should_be_blocked = scenario.verdict == Verdict::kBlock;
  int eof_packet = static_cast<int>(scenario.packets.size());
  int effective_verdict_packet = scenario.verdict_packet;
  if (should_be_blocked) {
    // Our implementation currently only blocks at the second OnWillRead.
    effective_verdict_packet = std::max(0, effective_verdict_packet);
  }

  for (int packet_index = 0;
       packet_index < static_cast<int>(packets_vector.size()); packet_index++) {
    const base::StringPiece packet = packets_vector[packet_index];
    SCOPED_TRACE(testing::Message()
                 << "While delivering packet #" << packet_index);
    bool should_be_streaming = scenario.verdict == Verdict::kAllow &&
                               packet_index > scenario.verdict_packet;

    if (packet_index <= effective_verdict_packet) {
      EXPECT_FALSE(document_blocker_->blocked_read_completed_);
      EXPECT_FALSE(document_blocker_->allow_based_on_sniffing_);
    } else {
      ASSERT_EQ(should_be_blocked, document_blocker_->blocked_read_completed_);
      EXPECT_EQ(!should_be_blocked && expected_to_sniff,
                document_blocker_->allow_based_on_sniffing_);
    }

    // Tell the ResourceHandlers to allocate the buffer for reading. On the
    // first read, and after an 'allow' verdict, this will request a buffer from
    // the downstream handler. Otherwise, it'll ask for more space in the local
    // buffer used for sniffing.
    mock_loader_->OnWillRead();

    if (should_be_blocked && packet_index == effective_verdict_packet + 1) {
      // The Cancel() occurs during the OnWillRead subsequent to the block
      // decision.
      EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
      break;
    }

    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
    EXPECT_NE(nullptr, mock_loader_->io_buffer());
    if (!should_be_streaming) {
      EXPECT_EQ(mock_loader_->io_buffer()->data(),
                document_blocker_->local_buffer_->data() +
                    document_blocker_->local_buffer_bytes_read_)
          << "Should have used a different IOBuffer for sniffing";
      EXPECT_NE(stream_sink_->buffer(), mock_loader_->io_buffer());
    } else {
      EXPECT_FALSE(should_be_blocked);
      EXPECT_EQ(mock_loader_->io_buffer(), stream_sink_->buffer())
          << "Should have used original IOBuffer when sniffing not needed";
    }

    // Deliver the next packet of the response body; this allows sniffing to
    // occur.
    mock_loader_->OnReadCompleted(packet);
    if (mock_loader_->status() ==
        MockResourceLoader::Status::CALLBACK_PENDING) {
      // CALLBACK_PENDING is only expected in the case when streaming starts.
      if (scenario.verdict_packet == kVerdictPacketForHeadersBasedVerdict) {
        // If not sniffing, then
        // - if response is allowed, then streaming should start in
        //   OnResponseStarted (and we shouldn't hit CALLBACK_PENDING state).
        // - if response is blocked, then CALLBACK_PENDING state will happen
        //   when enforcing blocking - at packet #0.
        EXPECT_EQ(Verdict::kBlock, scenario.verdict);
        EXPECT_EQ(0, packet_index);
      } else {
        // If sniffing, then streaming should start at the verdict packet.
        EXPECT_EQ(scenario.verdict_packet, packet_index);
      }

      // Waits for CrossSiteDocumentResourceHandler::Resume() if needed.
      mock_loader_->WaitUntilIdleOrCanceled();
    }
    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
    EXPECT_EQ(nullptr, mock_loader_->io_buffer());

    // mock_loader->OnWillRead() should only result in calls through to
    // stream_sink_->OnWillRead() for the first packet, or for packets after an
    // "allow" decision.
    EXPECT_EQ(GetExpectedNumberOfOnWillReadCalls(packet_index),
              stream_sink_->on_will_read_called());

    if (should_be_blocked) {
      ASSERT_FALSE(document_blocker_->allow_based_on_sniffing_);
      ASSERT_EQ("", stream_sink_body_)
          << "Response should not have been delivered to the renderer.";
      ASSERT_LE(packet_index, effective_verdict_packet);
      ASSERT_EQ(packet_index == effective_verdict_packet,
                document_blocker_->blocked_read_completed_);
    } else if (expected_to_sniff && packet_index >= scenario.verdict_packet) {
      ASSERT_TRUE(document_blocker_->allow_based_on_sniffing_);
      ASSERT_FALSE(document_blocker_->blocked_read_completed_);
    } else {
      ASSERT_FALSE(document_blocker_->allow_based_on_sniffing_);
      ASSERT_FALSE(document_blocker_->blocked_read_completed_);
    }
    if (should_be_streaming) {
      ASSERT_LE(packet.size(), stream_sink_body_.size());
      EXPECT_EQ(packet, stream_sink_body_.substr(stream_sink_body_.size() -
                                                 packet.size()))
          << "Response should be streamed to the renderer.";
    }
  }

  // All packets are now sent. Validate our final expectations, and send
  // OnResponseCompleted.
  EXPECT_EQ(GetExpectedNumberOfOnWillReadCalls(eof_packet),
            stream_sink_->on_will_read_called());

  // Check the final block/no-block decision.
  EXPECT_EQ(document_blocker_->blocked_read_completed_, should_be_blocked);
  EXPECT_EQ(document_blocker_->allow_based_on_sniffing_,
            !should_be_blocked && expected_to_sniff);

  // Simulate an OnResponseCompleted from the ResourceLoader.
  if (should_be_blocked) {
    if (!scenario.data().empty()) {
      // TODO(nick): We may be left in an inconsistent state when blocking an
      // empty nosniff response. Remove the above 'if'.
      EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
    }
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    EXPECT_EQ(stream_sink_body_, "");

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnResponseCompleted(status));
  } else {
    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
    EXPECT_EQ(stream_sink_body_, scenario.data());

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnResponseCompleted(
                  net::URLRequestStatus::FromError(net::OK)));
  }

  // Ensure that all or none of the data arrived.
  if (should_be_blocked)
    EXPECT_EQ(stream_sink_body_, "");
  else
    EXPECT_EQ(stream_sink_body_, scenario.data());

  // Verify that histograms are correctly incremented.
  base::HistogramTester::CountsMap expected_counts;
  std::string histogram_base = "SiteIsolation.XSD.Browser";
  switch (scenario.canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kPlain:
    case MimeType::kOthers:
      break;
    case MimeType::kNeverSniffed:
      DCHECK_EQ(Verdict::kBlock, scenario.verdict);
      DCHECK_EQ(-1, scenario.verdict_packet);
      break;
    case MimeType::kInvalidMimeType:
      DCHECK_EQ(Verdict::kAllow, scenario.verdict);
      DCHECK_EQ(-1, scenario.verdict_packet);
      break;
  }
  int start_action = static_cast<int>(
      network::CrossOriginReadBlocking::Action::kResponseStarted);
  int end_action = -1;
  if (should_be_blocked && expected_to_sniff) {
    end_action = static_cast<int>(
        network::CrossOriginReadBlocking::Action::kBlockedAfterSniffing);
  } else if (should_be_blocked && !expected_to_sniff) {
    end_action = static_cast<int>(
        network::CrossOriginReadBlocking::Action::kBlockedWithoutSniffing);
  } else if (!should_be_blocked && expected_to_sniff) {
    end_action = static_cast<int>(
        network::CrossOriginReadBlocking::Action::kAllowedAfterSniffing);
  } else if (!should_be_blocked && !expected_to_sniff) {
    end_action = static_cast<int>(
        network::CrossOriginReadBlocking::Action::kAllowedWithoutSniffing);
  } else {
    NOTREACHED();
  }

  // Expecting two actions: ResponseStarted and one of the outcomes.
  expected_counts[histogram_base + ".Action"] = 2;
  EXPECT_THAT(histograms.GetAllSamples(histogram_base + ".Action"),
              testing::ElementsAre(base::Bucket(start_action, 1),
                                   base::Bucket(end_action, 1)))
      << "Should have incremented the right actions.";
  if (should_be_blocked)
    expected_counts[histogram_base + ".Blocked.CanonicalMimeType"] = 1;
  // Make sure that the expected metrics, and only those metrics, were
  // incremented.
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser"),
              testing::ContainerEq(expected_counts));

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

// Similar to the ResponseBlocking test above, but simulates the case that the
// downstream handler does not immediately resume from OnWillRead, in which case
// the downstream buffer may not be allocated until later.
TEST_P(CrossSiteDocumentResourceHandlerTest, OnWillReadDefer) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  Initialize(scenario.target_url, scenario.resource_type,
             scenario.initiator_origin, scenario.cors_request,
             false /* = inject_mime_sniffer */);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  // Set up response based on scenario.
  scoped_refptr<network::ResourceResponse> response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));

  // Verify that we will sniff content into a different buffer if sniffing is
  // needed.  Note that the different buffer is used even for blocking cases
  // where no sniffing is needed, to avoid complexity in the handler.  The
  // handler doesn't look at the data in that case, but there's no way to verify
  // it in the test.
  bool expected_to_sniff =
      scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict;
  ASSERT_EQ(expected_to_sniff, document_blocker_->analyzer_->needs_sniffing());

  // Cause the TestResourceHandler to defer when OnWillRead is called, to make
  // sure the test scenarios still work when the downstream handler's buffer
  // isn't allocated in the same call.
  size_t bytes_delivered = 0;
  int buffer_requests = 0;
  int packets = 0;
  std::vector<const char*> packets_vector(scenario.packets);
  packets_vector.push_back("");
  for (base::StringPiece packet : packets_vector) {
    bool should_be_streaming = scenario.verdict == Verdict::kAllow &&
                               packets > scenario.verdict_packet;
    stream_sink_->set_defer_on_will_read(true);
    mock_loader_->OnWillRead();
    if (bytes_delivered == 0 || should_be_streaming) {
      ASSERT_EQ(++buffer_requests, stream_sink_->on_will_read_called());
      ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
                mock_loader_->status());

      // No buffers have been allocated yet.
      EXPECT_EQ(nullptr, mock_loader_->io_buffer());
      EXPECT_EQ(nullptr, document_blocker_->local_buffer_.get());

      // Resume the downstream handler, which should establish a buffer for the
      // ResourceLoader (either the downstream one or a local one for sniffing).
      stream_sink_->WaitUntilDeferred();
      stream_sink_->Resume();
    } else {
      if (document_blocker_->blocked_read_completed_) {
        // We've decided to block.
        EXPECT_EQ(Verdict::kBlock, scenario.verdict);
        EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
        break;
      } else {
        EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
      }
    }
    ASSERT_NE(nullptr, mock_loader_->io_buffer());

    if (!should_be_streaming) {
      ASSERT_NE(nullptr, document_blocker_->local_buffer_.get());
      EXPECT_EQ(mock_loader_->io_buffer()->data(),
                document_blocker_->local_buffer_->data() + bytes_delivered)
          << "Should have used a different IOBuffer for sniffing";
    } else {
      EXPECT_EQ(nullptr, document_blocker_->local_buffer_.get());
      EXPECT_EQ(mock_loader_->io_buffer(), stream_sink_->buffer())
          << "Should have used original IOBuffer when sniffing not needed";
    }
    // Deliver the next packet of the response body.
    mock_loader_->OnReadCompleted(packet);
    if (packet.empty() && (packets == scenario.verdict_packet) &&
        (bytes_delivered > 0)) {
      // This case will result in CrossSiteDocumentResourceHandler having to
      // synthesize an extra OnWillRead.
      stream_sink_->set_defer_on_will_read(true);
      stream_sink_->WaitUntilDeferred();
      stream_sink_->Resume();
    }
    if (mock_loader_->status() ==
        MockResourceLoader::Status::CALLBACK_PENDING) {
      // CALLBACK_PENDING is only expected in the case when streaming starts.
      if (scenario.verdict_packet == kVerdictPacketForHeadersBasedVerdict) {
        // If not sniffing, then
        // - if response is allowed, then streaming should start in
        //   OnResponseStarted (and we shouldn't hit CALLBACK_PENDING state).
        // - if response is blocked, then CALLBACK_PENDING state will happen
        //   when enforcing blocking - at packet #0.
        EXPECT_EQ(Verdict::kBlock, scenario.verdict);
        EXPECT_EQ(0, packets);
      } else {
        // If sniffing, then streaming should start at the verdict packet.
        EXPECT_EQ(scenario.verdict_packet, packets);
      }
      mock_loader_->WaitUntilIdleOrCanceled();
    }
    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

    if (document_blocker_->blocked_read_completed_) {
      EXPECT_EQ(Verdict::kBlock, scenario.verdict);
      ASSERT_EQ(packets, std::max(0, scenario.verdict_packet));
    }

    bytes_delivered += packet.size();
    packets++;

    EXPECT_EQ(nullptr, mock_loader_->io_buffer());
  }

  // Verify that the response is blocked or allowed as expected.
  if (scenario.verdict == Verdict::kBlock) {
    EXPECT_EQ("", stream_sink_body_)
        << "Response should not have been delivered to the renderer.";

    EXPECT_TRUE(document_blocker_->blocked_read_completed_);
    EXPECT_FALSE(document_blocker_->allow_based_on_sniffing_);
  } else {
    // Make sure that the response was delivered.
    EXPECT_EQ(scenario.data(), stream_sink_body_)
        << "Response should have been delivered to the renderer.";
    EXPECT_FALSE(document_blocker_->blocked_read_completed_);
    EXPECT_EQ(expected_to_sniff, document_blocker_->allow_based_on_sniffing_);
  }

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

// Runs a particular TestScenario (passed as the test's parameter) through the
// ResourceLoader, MimeSniffingResourceHandler and
// CrossSiteDocumentResourceHandler, verifying that the response is correctly
// allowed or blocked based on the scenario.
TEST_P(CrossSiteDocumentResourceHandlerTest, MimeSnifferInterop) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  Initialize(scenario.target_url, scenario.resource_type,
             scenario.initiator_origin, scenario.cors_request,
             true /* = inject_mime_sniffer */);
  base::HistogramTester histograms;

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  // Set up response based on scenario.
  scoped_refptr<network::ResourceResponse> response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  // Call OnResponseStarted.  Note that MimeSniffingResourceHandler will not
  // immediately forward the call to CrossSiteDocumentResourceHandler.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));

  // Calculate expectations, based on scenario properties.
  bool expected_to_sniff =
      scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict;
  bool should_be_blocked = scenario.verdict == Verdict::kBlock;
  bool expected_to_block_based_on_headers =
      expected_to_sniff || should_be_blocked;

  // Push all packets through.  Minimal verifications, because we don't want the
  // test to make assumptions about when exactly MimeSniffingResourceHandler
  // forwards the calls down to CrossSiteDocumentResourceHandler
  std::vector<const char*> packets_vector(scenario.packets);
  packets_vector.push_back("");  // End-of-stream is marked by an empty packet.
  for (int packet_index = 0;
       packet_index < static_cast<int>(packets_vector.size()); packet_index++) {
    const base::StringPiece packet = packets_vector[packet_index];
    SCOPED_TRACE(testing::Message()
                 << "While delivering packet #" << packet_index);
    mock_loader_->OnWillRead();
    if (mock_loader_->status() == MockResourceLoader::Status::CANCELED)
      break;
    if (mock_loader_->status() == MockResourceLoader::Status::CALLBACK_PENDING)
      mock_loader_->WaitUntilIdleOrCanceled();

    mock_loader_->OnReadCompleted(packet);
    if (mock_loader_->status() == MockResourceLoader::Status::CANCELED)
      break;
    if (mock_loader_->status() == MockResourceLoader::Status::CALLBACK_PENDING)
      mock_loader_->WaitUntilIdleOrCanceled();
  }

  // Call OnResponseCompleted.
  net::URLRequestStatus request_status;
  if (mock_loader_->status() == MockResourceLoader::Status::CANCELED) {
    request_status = net::URLRequestStatus(net::URLRequestStatus::CANCELED,
                                           net::ERR_ABORTED);
  } else {
    request_status = net::URLRequestStatus::FromError(net::OK);
  }
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(request_status));

  // At this point MimeSniffingResourceHandler should have forwarded all calls
  // down to CrossSiteDocumentResourceHandler - it is now okay to verify
  // CrossSiteDocumentResourceHandler's behavior.
  ASSERT_TRUE(document_blocker_->has_response_started_);
  EXPECT_EQ(scenario.canonical_mime_type,
            document_blocker_->analyzer_->canonical_mime_type_for_testing());
  EXPECT_EQ(expected_to_sniff, document_blocker_->analyzer_->needs_sniffing());
  ASSERT_EQ(expected_to_block_based_on_headers,
            document_blocker_->should_block_based_on_headers_);

  // Ensure that all or none of the data arrived.
  if (should_be_blocked)
    EXPECT_EQ("", stream_sink_body_);
  else
    EXPECT_EQ(scenario.data(), stream_sink_body_);

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

// Runs a particular TestScenario (passed as the test's parameter) through the
// ResourceLoader and CrossSiteDocumentResourceHandler, verifying that the
// expected CORB Protection UMA Histogram values are reported.
TEST_P(CrossSiteDocumentResourceHandlerTest, CORBProtectionLogging) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  Initialize(scenario.target_url, scenario.resource_type,
             scenario.initiator_origin, scenario.cors_request,
             false /* = inject_mime_sniffer */);
  base::HistogramTester histograms;

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  // Set up response based on scenario.
  scoped_refptr<network::ResourceResponse> response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  // Store if the response seems sensitive now, because CORB will clear its
  // headers if it decides to block.
  const bool seems_sensitive_from_cors_heuristic =
      network::CrossOriginReadBlocking::ResponseAnalyzer::
          SeemsSensitiveFromCORSHeuristic(response->head);
  const bool seems_sensitive_from_cache_heuristic =
      network::CrossOriginReadBlocking::ResponseAnalyzer::
          SeemsSensitiveFromCacheHeuristic(response->head);
  const bool expect_nosniff =
      network::CrossOriginReadBlocking::ResponseAnalyzer::HasNoSniff(
          response->head);

  // Call OnResponseStarted.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));

  bool should_be_blocked = scenario.verdict == Verdict::kBlock;
  int effective_verdict_packet = scenario.verdict_packet;
  if (should_be_blocked) {
    // Our implementation currently only blocks at the second OnWillRead.
    effective_verdict_packet = std::max(0, effective_verdict_packet);
  }

  // This vector holds the packets to be delivered.
  std::vector<const char*> packets_vector(scenario.packets);
  packets_vector.push_back("");  // End-of-stream is marked by an empty packet.
  for (int packet_index = 0;
       packet_index < static_cast<int>(packets_vector.size()); packet_index++) {
    const base::StringPiece packet = packets_vector[packet_index];
    SCOPED_TRACE(testing::Message()
                 << "While delivering packet #" << packet_index);
    mock_loader_->OnWillRead();
    if (should_be_blocked && packet_index == effective_verdict_packet + 1) {
      // The Cancel() occurs during the OnWillRead subsequent to the block
      // decision.
      break;
    }
    mock_loader_->OnReadCompleted(packet);

    if (mock_loader_->status() ==
        MockResourceLoader::Status::CALLBACK_PENDING) {
      // Waits for CrossSiteDocumentResourceHandler::Resume() if needed.
      mock_loader_->WaitUntilIdleOrCanceled();
    }
  }

  if (should_be_blocked) {
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnResponseCompleted(status));
  } else {
    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnResponseCompleted(
                  net::URLRequestStatus::FromError(net::OK)));
  }

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["SiteIsolation.CORBProtection.SensitiveResource"] = 1;
  if (scenario.resource_is_sensitive) {
    // Check that we reported correctly if the server supports range requests.
    bool supports_range_requests = network::CrossOriginReadBlocking::
        ResponseAnalyzer::SupportsRangeRequests(response->head);
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveWithRangeSupport"),
                testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
    expected_counts["SiteIsolation.CORBProtection.SensitiveWithRangeSupport"] =
        1;

    std::string mime_type_bucket;
    switch (scenario.mime_type_bucket) {
      case MimeTypeBucket::kProtected:
        mime_type_bucket = ".ProtectedMimeType";
        break;
      case MimeTypeBucket::kPublic:
        mime_type_bucket = ".PublicMimeType";
        break;
      case MimeTypeBucket::kOther:
        mime_type_bucket = ".OtherMimeType";
        break;
    }
    std::string blocked_with_range_support;
    switch (scenario.protection_decision) {
      case CrossOriginProtectionDecision::kBlock:
        blocked_with_range_support = ".BlockedWithRangeSupport";
        break;
      case CrossOriginProtectionDecision::kBlockedAfterSniffing:
        blocked_with_range_support = ".BlockedAfterSniffingWithRangeSupport";
        break;
      default:
        blocked_with_range_support = "This value should not be used.";
    }
    bool expect_range_support_histograms =
        scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
        (scenario.protection_decision ==
             CrossOriginProtectionDecision::kBlock ||
         scenario.protection_decision ==
             CrossOriginProtectionDecision::kBlockedAfterSniffing);
    // In this scenario the file seemed sensitive so we expect a report. Note
    // there may be two reports if the resource satisfied both the CORS
    // heuristic and the Cache heuristic.
    std::string cors_base = "SiteIsolation.CORBProtection.CORSHeuristic";
    std::string cache_base = "SiteIsolation.CORBProtection.CacheHeuristic";
    std::string cors_protected = cors_base + ".ProtectedMimeType";
    std::string cache_protected = cache_base + ".ProtectedMimeType";
    if (seems_sensitive_from_cors_heuristic) {
      expected_counts[cors_base + mime_type_bucket] = 1;
      EXPECT_THAT(histograms.GetAllSamples(cors_base + mime_type_bucket),
                  testing::ElementsAre(base::Bucket(
                      static_cast<int>(scenario.protection_decision), 1)))
          << "CORB should have reported the right protection decision.";
      if (expect_range_support_histograms) {
        EXPECT_THAT(
            histograms.GetAllSamples(cors_protected +
                                     blocked_with_range_support),
            testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
        expected_counts[cors_protected + blocked_with_range_support] = 1;
      }
      if (scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
          scenario.protection_decision ==
              CrossOriginProtectionDecision::kBlock) {
        EXPECT_THAT(histograms.GetAllSamples(
                        cors_protected + ".BlockedWithoutSniffing.HasNoSniff"),
                    testing::ElementsAre(base::Bucket(expect_nosniff, 1)));
        expected_counts[cors_protected + ".BlockedWithoutSniffing.HasNoSniff"] =
            1;
      }
    }
    if (seems_sensitive_from_cache_heuristic) {
      expected_counts[cache_base + mime_type_bucket] = 1;
      EXPECT_THAT(histograms.GetAllSamples(cache_base + mime_type_bucket),
                  testing::ElementsAre(base::Bucket(
                      static_cast<int>(scenario.protection_decision), 1)))
          << "CORB should have reported the right protection decision.";
      if (expect_range_support_histograms) {
        EXPECT_THAT(
            histograms.GetAllSamples(cache_protected +
                                     blocked_with_range_support),
            testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
        expected_counts[cache_protected + blocked_with_range_support] = 1;
      }
      if (scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
          scenario.protection_decision ==
              CrossOriginProtectionDecision::kBlock) {
        EXPECT_THAT(histograms.GetAllSamples(
                        cache_protected + ".BlockedWithoutSniffing.HasNoSniff"),
                    testing::ElementsAre(base::Bucket(expect_nosniff, 1)));
        expected_counts[cache_protected +
                        ".BlockedWithoutSniffing.HasNoSniff"] = 1;
      }
    }

    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("SiteIsolation.CORBProtection"),
        testing::ContainerEq(expected_counts));
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveResource"),
                testing::ElementsAre(base::Bucket(true, 1)));
  } else {
    // In this scenario the file should not have appeared sensitive, so only the
    // SensitiveResource boolean should have been reported (as false).
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("SiteIsolation.CORBProtection"),
        testing::ContainerEq(expected_counts));
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveResource"),
                testing::ElementsAre(base::Bucket(false, 1)));
  }

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

INSTANTIATE_TEST_SUITE_P(,
                         CrossSiteDocumentResourceHandlerTest,
                         ::testing::ValuesIn(kScenarios));

}  // namespace content

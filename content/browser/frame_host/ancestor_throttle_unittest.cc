// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using HeaderDisposition = AncestorThrottle::HeaderDisposition;

net::HttpResponseHeaders* GetAncestorHeaders(const char* xfo, const char* csp) {
  std::string header_string("HTTP/1.1 200 OK\nX-Frame-Options: ");
  header_string += xfo;
  if (csp != nullptr) {
    header_string += "\nContent-Security-Policy: ";
    header_string += csp;
  }
  header_string += "\n\n";
  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(header_string);
  EXPECT_TRUE(headers->HasHeader("X-Frame-Options"));
  if (csp != nullptr)
    EXPECT_TRUE(headers->HasHeader("Content-Security-Policy"));
  return headers;
}

}  // namespace

// AncestorThrottleTest
// -------------------------------------------------------------

class AncestorThrottleTest : public testing::Test {};

TEST_F(AncestorThrottleTest, ParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* value;
  } cases[] = {
      // Basic keywords
      {"DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL"},

      // Repeated keywords
      {"DENY,DENY", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL,ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL, ALLOWALL"},

      // Case-insensitive
      {"deNy", HeaderDisposition::DENY, "deNy"},
      {"sAmEorIgIn", HeaderDisposition::SAMEORIGIN, "sAmEorIgIn"},
      {"AlLOWaLL", HeaderDisposition::ALLOWALL, "AlLOWaLL"},

      // Trim whitespace
      {" DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {" ALLOWALL ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {"   DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN   ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"   ALLOWALL   ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {" DENY , DENY ", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,  SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL  ,ALLOWALL", HeaderDisposition::ALLOWALL,
       "ALLOWALL, ALLOWALL"},
  };

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.value, header_value);
  }
}

TEST_F(AncestorThrottleTest, ErrorsParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* failure;
  } cases[] = {
      // Empty == Invalid.
      {"", HeaderDisposition::INVALID, ""},

      // Invalid
      {"INVALID", HeaderDisposition::INVALID, "INVALID"},
      {"INVALID DENY", HeaderDisposition::INVALID, "INVALID DENY"},
      {"DENY DENY", HeaderDisposition::INVALID, "DENY DENY"},
      {"DE NY", HeaderDisposition::INVALID, "DE NY"},

      // Conflicts
      {"INVALID,DENY", HeaderDisposition::CONFLICT, "INVALID, DENY"},
      {"DENY,ALLOWALL", HeaderDisposition::CONFLICT, "DENY, ALLOWALL"},
      {"SAMEORIGIN,DENY", HeaderDisposition::CONFLICT, "SAMEORIGIN, DENY"},
      {"ALLOWALL,SAMEORIGIN", HeaderDisposition::CONFLICT,
       "ALLOWALL, SAMEORIGIN"},
      {"DENY,  SAMEORIGIN", HeaderDisposition::CONFLICT, "DENY, SAMEORIGIN"}};

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.failure, header_value);
  }
}

TEST_F(AncestorThrottleTest, AllowsBlanketEnforcementOfRequiredCSP) {
  if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE))
    return;

  struct TestCase {
    const char* name;
    const char* request_origin;
    const char* response_origin;
    const char* allow_csp_from;
    bool expected_result;
  } cases[] = {
      {
          "About scheme allows",
          "http://example.com",
          "about://me",
          nullptr,
          true,
      },
      {
          "File scheme allows",
          "http://example.com",
          "file://me",
          nullptr,
          true,
      },
      {
          "Data scheme allows",
          "http://example.com",
          "data://me",
          nullptr,
          true,
      },
      {
          "Filesystem scheme allows",
          "http://example.com",
          "filesystem://me",
          nullptr,
          true,
      },
      {
          "Blob scheme allows",
          "http://example.com",
          "blob://me",
          nullptr,
          true,
      },
      {
          "Same origin allows",
          "http://example.com",
          "http://example.com",
          nullptr,
          true,
      },
      {
          "Same origin allows independently of header",
          "http://example.com",
          "http://example.com",
          "http://not-example.com",
          true,
      },
      {
          "Different origin does not allow",
          "http://example.com",
          "http://not.example.com",
          nullptr,
          false,
      },
      {
          "Different origin with right header allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com",
          true,
      },
      {
          "Different origin with right header 2 allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com/",
          true,
      },
      {
          "Different origin with wrong header does not allow",
          "http://example.com",
          "http://not-example.com",
          "http://not-example.com",
          false,
      },
      {
          "Wildcard header allows",
          "http://example.com",
          "http://not-example.com",
          "*",
          true,
      },
      {
          "Malformed header does not allow",
          "http://example.com",
          "http://not-example.com",
          "*; http://example.com",
          false,
      },
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.allow_csp_from)
      headers->AddHeader("allow-csp-from", test.allow_csp_from);
    auto allow_csp_from = network::ParseAllowCSPFromHeader(*headers);

    bool actual = AncestorThrottle::AllowsBlanketEnforcementOfRequiredCSP(
        url::Origin::Create(GURL(test.request_origin)),
        GURL(test.response_origin), allow_csp_from);
    EXPECT_EQ(test.expected_result, actual);
  }
}

}  // namespace content

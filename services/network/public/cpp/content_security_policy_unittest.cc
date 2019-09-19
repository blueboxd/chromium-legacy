// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace network {

namespace {

struct ExpectedResult {
  size_t size;
  struct ParsedSource {
    std::string scheme;
    std::string host;
    int port;
    std::string path;
    bool is_host_wildcard;
    bool is_port_wildcard;
    bool allow_self;
  };
  std::vector<ParsedSource> parsed_sources;
};

struct TestData {
  std::string header;
  bool is_valid;
  ExpectedResult expected_result;

  TestData(const std::string& header, ExpectedResult expected_result)
      : header(header), is_valid(true), expected_result(expected_result) {}
  TestData(const std::string& header) : header(header), is_valid(false) {}
};

static void TestCSPParser(const std::string& header,
                          const ExpectedResult* expected_result) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->AddHeader("Content-Security-Policy: frame-ancestors " + header);
  ContentSecurityPolicy policy;
  policy.Parse(*headers);

  if (!expected_result) {
    EXPECT_FALSE(policy.content_security_policy_ptr());
    return;
  }
  auto& frame_ancestors = policy.content_security_policy_ptr()->frame_ancestors;
  EXPECT_EQ(frame_ancestors->sources.size(), expected_result->size);
  for (size_t i = 0; i < expected_result->parsed_sources.size(); i++) {
    EXPECT_EQ(frame_ancestors->sources[i]->scheme,
              expected_result->parsed_sources[i].scheme);
    EXPECT_EQ(frame_ancestors->sources[i]->host,
              expected_result->parsed_sources[i].host);
    EXPECT_EQ(frame_ancestors->sources[i]->port,
              expected_result->parsed_sources[i].port);
    EXPECT_EQ(frame_ancestors->sources[i]->path,
              expected_result->parsed_sources[i].path);
    EXPECT_EQ(frame_ancestors->sources[i]->is_host_wildcard,
              expected_result->parsed_sources[i].is_host_wildcard);
    EXPECT_EQ(frame_ancestors->sources[i]->is_port_wildcard,
              expected_result->parsed_sources[i].is_port_wildcard);
    EXPECT_EQ(frame_ancestors->sources[i]->allow_self,
              expected_result->parsed_sources[i].allow_self);
  }
}

}  // namespace

TEST(ContentSecurityPolicy, Parse) {
  TestData test_data[] = {
      // Parse scheme.
      // Empty scheme.
      {":"},

      // First character is alpha/non-alpha.
      {"a:", {1, {{"a", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"1ba:"},
      {"-:"},

      // Remaining characters.
      {"abcd:",
       {1, {{"abcd", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"a123:",
       {1, {{"a123", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"a+-:",
       {1, {{"a+-", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"a1+-:",
       {1, {{"a1+-", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},

      // Invalid character.
      {"wrong_scheme"},

      // Parse host.
      // Wildcards.
      {"*", {1, {{"", "", url::PORT_UNSPECIFIED, "", true, false, false}}}},
      {"*."},
      {"*.a", {1, {{"", "a", url::PORT_UNSPECIFIED, "", true, false, false}}}},
      {"a.*"},
      {"a.*.b"},
      {"*a"},

      // Dot separation.
      {"a", {1, {{"", "a", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"a.b.c",
       {1, {{"", "a.b.c", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"a.b."},
      {".b.c"},
      {"a..c"},

      // Valid/Invalid characters.
      {"az09-",
       {1, {{"", "az09-", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"+"},

      // Strange host.
      {"---.com",
       {1, {{"", "---.com", url::PORT_UNSPECIFIED, "", false, false, false}}}},

      // Parse port.
      // Empty port.
      {"scheme://host:"},

      // Common case.
      {"a:80", {1, {{"", "a", 80, "", false, false, false}}}},

      // Wildcard port.
      {"a:*", {1, {{"", "a", url::PORT_UNSPECIFIED, "", false, true, false}}}},

      // Leading zeroes.
      {"a:000", {1, {{"", "a", 0, "", false, false, false}}}},
      {"a:0", {1, {{"", "a", 0, "", false, false, false}}}},

      // Invalid chars.
      {"a:-1"},
      {"a:+1"},

      // Parse path.
      // Encoded.
      {"example.com/%48%65%6c%6c%6f%20%57%6f%72%6c%64",
       {1,
        {{"", "example.com", url::PORT_UNSPECIFIED, "/Hello World", false,
          false, false}}}},

      // Other.
      {"*:*", {1, {{"", "", url::PORT_UNSPECIFIED, "", true, true, false}}}},
      {"http:",
       {1, {{"http", "", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"https://*",
       {1, {{"https", "", url::PORT_UNSPECIFIED, "", true, false, false}}}},
      {"http:/example.com"},
      {"http://"},
      {"example.com",
       {1,
        {{"", "example.com", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"example.com/path",
       {1,
        {{"", "example.com", url::PORT_UNSPECIFIED, "/path", false, false,
          false}}}},
      {"https://example.com",
       {1,
        {{"https", "example.com", url::PORT_UNSPECIFIED, "", false, false,
          false}}}},
      {"https://example.com/path",
       {1,
        {{"https", "example.com", url::PORT_UNSPECIFIED, "/path", false, false,
          false}}}},
      {"https://example.com:1234",
       {1, {{"https", "example.com", 1234, "", false, false, false}}}},
      {"https://example.com:2345/some/path",
       {1,
        {{"https", "example.com", 2345, "/some/path", false, false, false}}}},
      {"example.com example.org",
       {2,
        {{"", "example.com", url::PORT_UNSPECIFIED, "", false, false, false},
         {"", "example.org", url::PORT_UNSPECIFIED, "", false, false, false}}}},
      {"about:blank"},
      {"'none'", {0, {}}},
      {"'self'",
       {1, {{"", "", url::PORT_UNSPECIFIED, "", false, false, true}}}},
      {"example.com 'none'"},
      {""},
  };

  for (auto& test : test_data)
    TestCSPParser(test.header, test.is_valid ? &test.expected_result : nullptr);
}

TEST(ContentSecurityPolicy, ParseMultipleDirectives) {
  // First directive is valid, second one is ignored.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: frame-ancestors example.com; other_directive "
        "value; frame-ancestors example.org");
    ContentSecurityPolicy policy;
    policy.Parse(*headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->allow_self, false);
  }

  // Skip the first directive, which is not frame-ancestors.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: other_directive value; frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(*headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->allow_self, false);
  }

  // Multiple CSP headers with multiple frame-ancestors directives present. Only
  // the first one is considered.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader("Content-Security-Policy: frame-ancestors example.com");
    headers->AddHeader("Content-Security-Policy: frame-ancestors example.org");
    ContentSecurityPolicy policy;
    policy.Parse(*headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->allow_self, false);
  }

  // Multiple CSP headers separated by ',' (RFC2616 section 4.2).
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: other_directive value, frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(*headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.org");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->allow_self, false);
  }

  // Multiple CSP headers separated by ',', with multiple frame-ancestors
  // directives present. Only the first one is considered.
  {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    headers->AddHeader(
        "Content-Security-Policy: frame-ancestors example.com, frame-ancestors "
        "example.org");
    ContentSecurityPolicy policy;
    policy.Parse(*headers);

    auto& frame_ancestors =
        policy.content_security_policy_ptr()->frame_ancestors;
    EXPECT_EQ(frame_ancestors->sources.size(), 1U);
    EXPECT_EQ(frame_ancestors->sources[0]->scheme, "");
    EXPECT_EQ(frame_ancestors->sources[0]->host, "example.com");
    EXPECT_EQ(frame_ancestors->sources[0]->port, url::PORT_UNSPECIFIED);
    EXPECT_EQ(frame_ancestors->sources[0]->path, "");
    EXPECT_EQ(frame_ancestors->sources[0]->is_host_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->is_port_wildcard, false);
    EXPECT_EQ(frame_ancestors->sources[0]->allow_self, false);
  }
}

}  // namespace network

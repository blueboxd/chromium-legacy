// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_package/signed_exchange_request_matcher.h"

#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

constexpr char kVariantsHeader[] = "variants-04";
constexpr char kVariantKeyHeader[] = "variant-key-04";

TEST(SignedExchangeRequestMatcherTest, CacheBehavior) {
  const struct TestCase {
    const char* name;
    std::map<std::string, std::string> req_headers;
    http_structured_header::ListOfLists variants;
    std::vector<std::vector<std::string>> expected;
  } cases[] = {
      // Accept
      {"vanilla content-type",
       {{"accept", "text/html"}},
       {{"Accept", "text/html"}},
       {{"text/html"}}},
      {"client supports two content-types",
       {{"accept", "text/html, image/jpeg"}},
       {{"Accept", "text/html"}},
       {{"text/html"}}},
      {"format miss",
       {{"accept", "image/jpeg"}},
       {{"Accept", "text/html"}},
       {{"text/html"}}},
      {"no format preference", {}, {{"Accept", "text/html"}}, {{"text/html"}}},
      {"no available format", {{"accept", "text/html"}}, {{"Accept"}}, {{}}},
      {"accept all types",
       {{"accept", "*/*"}},
       {{"Accept", "text/html", "image/jpeg"}},
       {{"text/html", "image/jpeg"}}},
      {"accept all subtypes",
       {{"accept", "image/*"}},
       {{"Accept", "text/html", "image/jpeg"}},
       {{"image/jpeg"}}},
      {"type params match",
       {{"accept", "text/html;param=bar"}},
       {{"Accept", "text/html;param=foo", "text/html;param=bar"}},
       {{"text/html;param=bar"}}},
      {"type with q value",
       {{"accept", "text/html;q=0.8;param=foo"}},
       {{"Accept", "image/jpeg", "text/html;param=foo"}},
       {{"text/html;param=foo"}}},
      {"type with zero q value",
       {{"accept", "text/html;q=0.0, image/jpeg"}},
       {{"Accept", "text/html", "image/jpeg"}},
       {{"image/jpeg"}}},
      {"type with invalid q value",
       {{"accept", "text/html;q=999, image/jpeg"}},
       {{"Accept", "text/html", "image/jpeg"}},
       {{"text/html", "image/jpeg"}}},
      // Accept-Encoding
      {"vanilla encoding",
       {{"accept-encoding", "gzip"}},
       {{"Accept-Encoding", "gzip"}},
       {{"gzip", "identity"}}},
      {"client supports two encodings",
       {{"accept-encoding", "gzip, br"}},
       {{"Accept-Encoding", "gzip"}},
       {{"gzip", "identity"}}},
      {"two stored, two preferences",
       {{"accept-encoding", "gzip, br"}},
       {{"Accept-Encoding", "gzip", "br"}},
       {{"gzip", "br", "identity"}}},
      {"no encoding preference",
       {},
       {{"Accept-Encoding", "gzip"}},
       {{"identity"}}},
      // Accept-Language
      {"vanilla language",
       {{"accept-language", "en"}},
       {{"Accept-Language", "en"}},
       {{"en"}}},
      {"multiple languages",
       {{"accept-language", "en, JA"}},
       {{"Accept-Language", "en", "fr", "ja"}},
       {{"en", "ja"}}},
      {"no language preference",
       {},
       {{"Accept-Language", "en", "ja"}},
       {{"en"}}},
      {"no available language",
       {{"accept-language", "en"}},
       {{"Accept-Language"}},
       {{}}},
      {"accept all languages",
       {{"accept-language", "*"}},
       {{"Accept-Language", "en", "ja"}},
       {{"en", "ja"}}},
      {"language subtag",
       {{"accept-language", "en"}},
       {{"Accept-Language", "en-US", "enq"}},
       {{"en-US"}}},
      {"language with q values",
       {{"accept-language", "ja, en;q=0.8"}},
       {{"Accept-Language", "fr", "en", "ja"}},
       {{"ja", "en"}}},
      {"language with zero q value",
       {{"accept-language", "ja, en;q=0"}},
       {{"Accept-Language", "fr", "en"}},
       {{"fr"}}},
      // Multiple axis
      {"format and language matches",
       {{"accept", "text/html"}, {"accept-language", "en"}},
       {{"Accept", "text/html"}, {"Accept-Language", "en", "fr"}},
       {{"text/html"}, {"en"}}},
      {"accept anything",
       {{"accept", "*/*"}, {"accept-language", "*"}},
       {{"Accept", "text/html", "image/jpeg"}, {"Accept-Language", "en", "fr"}},
       {{"text/html", "image/jpeg"}, {"en", "fr"}}},
      {"unknown field name",
       {{"accept-language", "en"}, {"unknown", "foo"}},
       {{"Accept-Language", "en"}, {"Unknown", "foo"}},
       {{"en"}}},
  };
  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    EXPECT_EQ(c.expected, SignedExchangeRequestMatcher::CacheBehavior(
                              c.variants, request_headers))
        << c.name;
  }
}

TEST(SignedExchangeRequestMatcherTest, MatchRequest) {
  const struct TestCase {
    const char* name;
    std::map<std::string, std::string> req_headers;
    SignedExchangeRequestMatcher::HeaderMap res_headers;
    bool should_match;
  } cases[] = {
      {"no variants and variant-key", {{"accept", "text/html"}}, {}, true},
      {"has variants but no variant-key",
       {{"accept", "text/html"}},
       {{kVariantsHeader, "Accept; text/html"}},
       false},
      {"has variant-key but no variants",
       {{"accept", "text/html"}},
       {{kVariantKeyHeader, "text/html"}},
       false},
      {"content type matches",
       {{"accept", "text/html"}},
       {{kVariantsHeader, "Accept; text/html; image/jpeg"},
        {kVariantKeyHeader, "text/html"}},
       true},
      {"content type misses",
       {{"accept", "image/jpeg"}},
       {{kVariantsHeader, "Accept; text/html; image/jpeg"},
        {kVariantKeyHeader, "text/html"}},
       false},
      {"encoding matches",
       {},
       {{kVariantsHeader, "Accept-Encoding;gzip;identity"},
        {kVariantKeyHeader, "identity"}},
       true},
      {"encoding misses",
       {},
       {{kVariantsHeader, "Accept-Encoding;gzip;identity"},
        {kVariantKeyHeader, "gzip"}},
       false},
      {"language matches",
       {{"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;en;ja"}, {kVariantKeyHeader, "en"}},
       true},
      {"language misses",
       {{"accept-language", "ja"}},
       {{kVariantsHeader, "Accept-Language;en;ja"}, {kVariantKeyHeader, "en"}},
       false},
      {"content type and language match",
       {{"accept", "text/html"}, {"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       true},
      {"content type matches but language misses",
       {{"accept", "text/html"}, {"accept-language", "fr"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       false},
      {"language matches but content type misses",
       {{"accept", "text/plain"}, {"accept-language", "en"}},
       {{kVariantsHeader, "Accept-Language;fr;en, Accept;text/plain;text/html"},
        {kVariantKeyHeader, "en;text/html"}},
       false},
      {"multiple variant key",
       {{"accept-encoding", "identity"}, {"accept-language", "fr"}},
       {{kVariantsHeader, "Accept-Encoding;gzip;br, Accept-Language;en;fr"},
        {kVariantKeyHeader, "gzip;fr, identity;fr"}},
       true},
      {"bad variant key item length",
       {},
       {{kVariantsHeader, "Accept;text/html, Accept-Language;en;fr"},
        {kVariantKeyHeader, "text/html;en, text/html;fr;oops"}},
       false},
      {"unknown field name",
       {{"accept-language", "en"}, {"unknown", "foo"}},
       {{kVariantsHeader, "Accept-Language;en, Unknown;foo"},
        {kVariantKeyHeader, "en;foo"}},
       false},
  };
  for (const auto& c : cases) {
    net::HttpRequestHeaders request_headers;
    for (auto it = c.req_headers.begin(); it != c.req_headers.end(); ++it)
      request_headers.SetHeader(it->first, it->second);
    EXPECT_EQ(c.should_match, SignedExchangeRequestMatcher::MatchRequest(
                                  request_headers, c.res_headers))
        << c.name;
  }
}

}  // namespace blink

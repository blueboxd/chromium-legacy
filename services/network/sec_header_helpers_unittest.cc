// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

constexpr char kSecureSite[] = "https://site.tld";
constexpr char kInsecureSite[] = "http://othersite.tld";

constexpr char kKnownSecChHeader[] = "Sec-CH-UA";
constexpr char kKnownSecFetchSiteHeader[] = "Sec-Fetch-Site";
constexpr char kKnownSecFetchModeHeader[] = "Sec-Fetch-Mode";
constexpr char kKnownSecFetchUserHeader[] = "Sec-Fetch-User";
constexpr char kOtherSecHeader[] = "sec-other-info-header";
constexpr char kOtherHeader[] = "Other-Header";

constexpr char kHeaderValue[] = "testdata";

}  // namespace

namespace network {

class SecHeaderHelpersTest : public PlatformTest {
 public:
  SecHeaderHelpersTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        url_request_(context_.CreateRequest(GURL(kSecureSite),
                                            net::DEFAULT_PRIORITY,
                                            /*request_delegate=*/nullptr,
                                            TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  net::URLRequest* url_request() const { return url_request_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  net::TestURLRequestContext context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

// Validate that Sec- prefixed headers are all removed when a request is
// downgraded from trustworthy to not such as when an https => http redirect
// occurs. We should only remove sec-ch- and sec-fetch- prefixed headers. Others
// should remain as they may be valid in an insecure context.
TEST_F(SecHeaderHelpersTest, SecHeadersRemovedOnDowngrade) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));
  ASSERT_EQ(2, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchSiteHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherSecHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
}

// Validate that if no downgrade occurs any Sec- prefixed headers remain on the
// provided request.
TEST_F(SecHeaderHelpersTest, SecHeadersRemainOnSecureRedirect) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchSiteHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherSecHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
}

// Validate that if Sec- headers exist as the first or last entries we properly
// remove them also.
TEST_F(SecHeaderHelpersTest, SecHeadersRemoveFirstLast) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(3, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));
  ASSERT_EQ(1, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchSiteHeader, &header_value));
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kOtherHeader, &header_value));
  ASSERT_FALSE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecChHeader, &header_value));
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with
// unprivileged requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, UnprivilegedRequestOnExtension) {
  net::URLRequest* current_url_request = url_request();
  GURL url = GURL(kSecureSite);

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;
  params.factory_bound_access_patterns =
      network::mojom::CorsOriginAccessPatterns::New();
  params.factory_bound_access_patterns->source_origin =
      url::Origin::Create(url);

  SetFetchMetadataHeaders(current_url_request,
                          network::mojom::RequestMode::kCors, false, &url,
                          params);
  ASSERT_EQ(2, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchSiteHeader, &header_value));
  ASSERT_EQ(header_value, "cross-site");

  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchModeHeader, &header_value));
  ASSERT_EQ(header_value, "cors");
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with privileged
// requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, PrivilegedRequestOnExtension) {
  net::URLRequest* current_url_request = url_request();
  GURL url = GURL(kSecureSite);

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;
  params.factory_bound_access_patterns =
      network::mojom::CorsOriginAccessPatterns::New();
  params.factory_bound_access_patterns->source_origin =
      url::Origin::Create(url);
  params.factory_bound_access_patterns->allow_patterns.push_back(
      mojom::CorsOriginPattern::New(
          url.scheme(), url.host(), 0,
          mojom::CorsDomainMatchMode::kDisallowSubdomains,
          mojom::CorsPortMatchMode::kAllowAnyPort,
          mojom::CorsOriginAccessMatchPriority::kDefaultPriority));

  SetFetchMetadataHeaders(current_url_request,
                          network::mojom::RequestMode::kCors, true, &url,
                          params);
  ASSERT_EQ(3, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  std::string header_value;
  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchSiteHeader, &header_value));
  ASSERT_EQ(header_value, "none");

  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchModeHeader, &header_value));
  ASSERT_EQ(header_value, "cors");

  ASSERT_TRUE(current_url_request->extra_request_headers().GetHeader(
      kKnownSecFetchUserHeader, &header_value));
  ASSERT_EQ(header_value, "?1");
}

}  // namespace network

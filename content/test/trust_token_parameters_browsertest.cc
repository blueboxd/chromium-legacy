// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// These integration tests verify that calling the Fetch API with Trust Tokens
// parameters results in the parameters' counterparts appearing downstream in
// network::ResourceRequest.
//
// Separately, Blink layout tests check that the API correctly rejects invalid
// input.

namespace content {

class TrustTokenParametersBrowsertest
    : public ::testing::WithParamInterface<network::TrustTokenTestParameters>,
      public ContentBrowserTest {
 public:
  TrustTokenParametersBrowsertest() {
    features_.InitAndEnableFeature(network::features::kTrustTokens);
  }

 protected:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    WithIssuanceParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kIssuanceTrustTokenTestParameters));

INSTANTIATE_TEST_SUITE_P(
    WithRedemptionParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kRedemptionTrustTokenTestParameters));

INSTANTIATE_TEST_SUITE_P(
    WithSigningParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kSigningTrustTokenTestParameters));

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("fetch($1, {trustToken: ", trust_token_url) +
                          expected_params_and_serialization.serialized_params +
                          "});"));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->trust_token_params);
  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("let iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "iframe.trustToken = $2;"
                         "document.body.appendChild(iframe);",
                         trust_token_url,
                         expected_params_and_serialization.serialized_params)));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->trust_token_params);
  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaXhr) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(),
             base::StringPrintf(
                 JsReplace("let request = new XMLHttpRequest();"
                           "request.open($1, $2);"
                           "request.setTrustToken(%s);"
                           "request.send();",
                           "GET", trust_token_url)
                     .c_str(),
                 expected_params_and_serialization.serialized_params.c_str())));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

}  // namespace content

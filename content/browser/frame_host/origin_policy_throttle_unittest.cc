// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const network::OriginPolicyContentsPtr kExamplePolicyContents =
    std::make_unique<network::OriginPolicyContents>(
        std::vector<std::string>(
            {"geolocation http://example.com"}) /* features */,
        std::vector<std::string>() /* content_security_policies */,
        std::vector<std::string>() /* content_security_policies_report_only */);
}

namespace content {

class OriginPolicyThrottleTest : public RenderViewHostTestHarness,
                                 public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    // Some tests below should be run with the feature en- and disabled, since
    // they test the feature functionality when enabled and feature
    // non-funcionality (that is, that the feature is inert) when disabled.
    // Hence, we run this test in both variants.
    features_.InitWithFeatureState(features::kOriginPolicy, GetParam());

    RenderViewHostTestHarness::SetUp();
  }
  void TearDown() override {
    nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }
  bool enabled() {
    return base::FeatureList::IsEnabled(features::kOriginPolicy);
  }

  void CreateHandleFor(const GURL& url) {
    net::HttpRequestHeaders headers;
    if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url))
      headers.SetHeader(net::HttpRequestHeaders::kSecOriginPolicy, "0");

    nav_handle_ = std::make_unique<MockNavigationHandle>(web_contents());
    nav_handle_->set_url(url);
    nav_handle_->set_request_headers(headers);
  }

 protected:
  std::unique_ptr<MockNavigationHandle> nav_handle_;
  base::test::ScopedFeatureList features_;
};

class TestOriginPolicyManager : public network::mojom::OriginPolicyManager {
 public:
  void RetrieveOriginPolicy(const url::Origin& origin,
                            const std::string& header_value,
                            RetrieveOriginPolicyCallback callback) override {
    network::OriginPolicy result;

    if (origin_exceptions_.find(origin) == origin_exceptions_.end()) {
      result.state = network::OriginPolicyState::kLoaded;
      result.contents = kExamplePolicyContents->ClonePtr();
      result.policy_url = origin.GetURL();
    } else {
      result.state = network::OriginPolicyState::kNoPolicyApplies;
      result.policy_url = origin.GetURL();
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestOriginPolicyManager::RunCallback,
                       base::Unretained(this), std::move(callback), result));
  }

  void RunCallback(RetrieveOriginPolicyCallback callback,
                   const network::OriginPolicy& result) {
    std::move(callback).Run(result);
  }

  network::mojom::OriginPolicyManagerPtr GetPtr() {
    network::mojom::OriginPolicyManagerPtr ptr;
    auto request = mojo::MakeRequest(&ptr);
    binding_ =
        std::make_unique<mojo::Binding<network::mojom::OriginPolicyManager>>(
            this, std::move(request));

    return ptr;
  }

  void AddExceptionFor(const url::Origin& origin) override {
    origin_exceptions_.insert(origin);
  }

 private:
  std::unique_ptr<mojo::Binding<network::mojom::OriginPolicyManager>> binding_;
  std::set<url::Origin> origin_exceptions_;
};

INSTANTIATE_TEST_SUITE_P(OriginPolicyThrottleTests,
                         OriginPolicyThrottleTest,
                         testing::Bool());

TEST_P(OriginPolicyThrottleTest, ShouldRequestOriginPolicy) {
  struct {
    const char* url;
    bool expect;
  } test_cases[] = {
      {"https://example.org/bla", true},
      {"http://example.org/bla", false},
      {"file:///etc/passwd", false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "URL: " << test_case.url);
    EXPECT_EQ(
        enabled() && test_case.expect,
        OriginPolicyThrottle::ShouldRequestOriginPolicy(GURL(test_case.url)));
  }
}

TEST_P(OriginPolicyThrottleTest, MaybeCreateThrottleFor) {
  CreateHandleFor(GURL("https://example.org/bla"));
  EXPECT_EQ(enabled(),
            !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));

  CreateHandleFor(GURL("http://insecure.org/bla"));
  EXPECT_FALSE(
      !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));
}

TEST_P(OriginPolicyThrottleTest, RunRequestEndToEnd) {
  if (!enabled())
    return;

  // Start the navigation.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.org/bla"), web_contents());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            navigation->GetLastThrottleCheckResult().action());

  // Fake a response with a policy header. Check whether the navigation
  // is deferred.
  const char* raw_headers =
      "HTTP/1.1 200 OK\nSec-Origin-Policy: policy=policy-1\n\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers));

  // We set a test origin policy manager as during unit tests we can't reach
  // the network service to retrieve a valid origin policy manager.
  TestOriginPolicyManager test_origin_policy_manager;
  NavigationHandleImpl* nav_handle =
      static_cast<NavigationHandleImpl*>(navigation->GetNavigationHandle());
  SiteInstance* site_instance = nav_handle->GetStartingSiteInstance();
  static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                          site_instance))
      ->SetOriginPolicyManagerForBrowserProcessForTesting(
          test_origin_policy_manager.GetPtr());

  nav_handle->set_response_headers_for_testing(headers);
  navigation->ReadyToCommit();
  EXPECT_TRUE(navigation->IsDeferred());
  OriginPolicyThrottle* policy_throttle = static_cast<OriginPolicyThrottle*>(
      nav_handle->GetDeferringThrottleForTesting());
  EXPECT_TRUE(policy_throttle);

  // Wait until the navigation has been allowed to proceed.
  navigation->Wait();

  // At the end of the navigation, the navigation handle should have a copy
  // of the origin policy.
  EXPECT_TRUE(nav_handle->navigation_request()
                  ->response()
                  ->head.origin_policy.has_value());
  EXPECT_EQ(kExamplePolicyContents, nav_handle->navigation_request()
                                        ->response()
                                        ->head.origin_policy.value()
                                        .contents);
  static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                          site_instance))
      ->ResetOriginPolicyManagerForBrowserProcessForTesting();
}

TEST_P(OriginPolicyThrottleTest, AddExceptionEndToEnd) {
  if (!enabled())
    return;

  // Start the navigation.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.org/bla"), web_contents());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  EXPECT_FALSE(navigation->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            navigation->GetLastThrottleCheckResult().action());

  // We set a test origin policy manager as during unit tests we can't reach
  // the network service to retrieve a valid origin policy manager.
  TestOriginPolicyManager test_origin_policy_manager;
  test_origin_policy_manager.AddExceptionFor(
      url::Origin::Create(GURL("https://example.org/blubb")));
  NavigationHandleImpl* nav_handle =
      static_cast<NavigationHandleImpl*>(navigation->GetNavigationHandle());
  SiteInstance* site_instance = nav_handle->GetStartingSiteInstance();
  static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                          site_instance))
      ->SetOriginPolicyManagerForBrowserProcessForTesting(
          test_origin_policy_manager.GetPtr());

  // Fake a response with a policy header.
  const char* raw_headers =
      "HTTP/1.1 200 OK\nSec-Origin-Policy: policy=policy-1\n\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers));
  nav_handle->set_response_headers_for_testing(headers);
  navigation->ReadyToCommit();

  // The policy manager has to be called even though this is an exception
  // because the throttle has no way of knowing that.
  EXPECT_TRUE(navigation->IsDeferred());
  OriginPolicyThrottle* policy_throttle = static_cast<OriginPolicyThrottle*>(
      nav_handle->GetDeferringThrottleForTesting());
  EXPECT_TRUE(policy_throttle);

  // Wait until the navigation has been allowed to proceed.
  navigation->Wait();

  // At the end of the navigation, the navigation handle should have no policy
  // as this origin should be exempted.
  EXPECT_FALSE(nav_handle->navigation_request()
                   ->response()
                   ->head.origin_policy.has_value());

  static_cast<StoragePartitionImpl*>(
      BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                          site_instance))
      ->ResetOriginPolicyManagerForBrowserProcessForTesting();
}

}  // namespace content

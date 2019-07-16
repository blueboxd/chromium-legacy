// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwBrowserContextTest : public testing::Test {
 protected:
  void SetUp() override {
    mojo::core::Init();
    feature_list_.InitAndEnableFeature(network::features::kNetworkService);
  }

  base::test::ScopedFeatureList feature_list_;
  // Create the TestBrowserThreads.
  content::TestBrowserThreadBundle thread_bundle_;
};

// Tests that constraints on trust for Symantec-issued certificates are not
// enforced for the NetworkContext, as it should behave like the Android system.
TEST_F(AwBrowserContextTest, SymantecPoliciesExempted) {
  AwBrowserContext context;
  network::mojom::NetworkContextParamsPtr network_context_params =
      context.GetNetworkContextParams(false, base::FilePath());

  ASSERT_TRUE(network_context_params);
  ASSERT_TRUE(network_context_params->initial_ssl_config);
  ASSERT_TRUE(network_context_params->initial_ssl_config
                  ->symantec_enforcement_disabled);
}

// Tests that SHA-1 is still allowed for locally-installed trust anchors,
// including those in application manifests, as it should behave like
// the Android system.
TEST_F(AwBrowserContextTest, SHA1LocalAnchorsAllowed) {
  AwBrowserContext context;
  network::mojom::NetworkContextParamsPtr network_context_params =
      context.GetNetworkContextParams(false, base::FilePath());

  ASSERT_TRUE(network_context_params);
  ASSERT_TRUE(network_context_params->initial_ssl_config);
  ASSERT_TRUE(
      network_context_params->initial_ssl_config->sha1_local_anchors_enabled);
}

}  // namespace android_webview

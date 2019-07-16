// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    mojo::core::Init();
    feature_list_.InitAndEnableFeature(network::features::kNetworkService);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AwContentBrowserClientTest, DisableCreatingThreadPool) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  EXPECT_TRUE(client.ShouldCreateThreadPool());

  AwContentBrowserClient::DisableCreatingThreadPool();
  EXPECT_FALSE(client.ShouldCreateThreadPool());
}

}  // namespace android_webview

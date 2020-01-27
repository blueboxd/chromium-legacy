// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

int kRenderFrameID = 1;

class TestSubresourceRedirectURLLoaderThrottle
    : public SubresourceRedirectURLLoaderThrottle {
 public:
  explicit TestSubresourceRedirectURLLoaderThrottle(
      std::vector<std::string> public_image_urls)
      : SubresourceRedirectURLLoaderThrottle(kRenderFrameID) {
    subresource_redirect_hints_agent_.SetCompressPublicImagesHints(
        blink::mojom::CompressPublicImagesHints::New(public_image_urls));
  }

  SubresourceRedirectHintsAgent* GetSubresourceRedirectHintsAgent() override {
    return &subresource_redirect_hints_agent_;
  }

 private:
  SubresourceRedirectHintsAgent subresource_redirect_hints_agent_;
};

namespace {

std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
CreateSubresourceRedirectURLLoaderThrottle(
    const GURL& url,
    content::ResourceType resource_type,
    int previews_state,
    const std::vector<std::string>& public_image_urls) {
  blink::WebURLRequest request;
  request.SetUrl(url);
  request.SetPreviewsState(previews_state);
  DCHECK(SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
             request, resource_type, kRenderFrameID)
             .get() != nullptr);

  return std::make_unique<TestSubresourceRedirectURLLoaderThrottle>(
      public_image_urls);
}

TEST(SubresourceRedirectURLLoaderThrottleTest, TestMaybeCreateThrottle) {
  struct TestCase {
    bool is_subresource_redirect_feature_enabled;
    content::ResourceType resource_type;
    int previews_state;
    const std::string url;
    bool expected_is_throttle_created;
  };

  const TestCase kTestCases[]{
      {true, content::ResourceType::kImage,
       content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", true},
      {true, content::ResourceType::kImage,
       content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON |
           content::PreviewsTypes::LAZY_IMAGE_AUTO_RELOAD,
       "https://www.test.com/test.jpg", true},

      // Failure cases
      {false, content::ResourceType::kImage,
       content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, content::ResourceType::kScript,
       content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, content::ResourceType::kImage,
       content::PreviewsTypes::LAZY_IMAGE_AUTO_RELOAD,
       "https://www.test.com/test.jpg", false},
      {true, content::ResourceType::kImage,
       content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "http://www.test.com/test.jpg", false},
  };

  for (const TestCase& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.is_subresource_redirect_feature_enabled) {
      scoped_feature_list.InitWithFeaturesAndParameters(
          {{blink::features::kSubresourceRedirect,
            {{"enable_lite_page_redirect", "true"}}}},
          {});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          blink::features::kSubresourceRedirect);
    }

    blink::WebURLRequest request;
    request.SetPreviewsState(test_case.previews_state);
    request.SetUrl(GURL(test_case.url));
    EXPECT_EQ(test_case.expected_is_throttle_created,
              SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
                  request, test_case.resource_type, kRenderFrameID) != nullptr);
  }
}

TEST(SubresourceRedirectURLLoaderThrottleTest, TestGetSubresourceURL) {
  struct TestCase {
    GURL original_url;
    GURL redirected_subresource_url;  // Empty URL means there will be no
                                      // redirect.
  };

  const TestCase kTestCases[]{
      {
          GURL("https://www.test.com/public_img.jpg"),
          GetSubresourceURLForURL(GURL("https://www.test.com/public_img.jpg")),
      },
      {
          GURL("https://www.test.com/public_img.jpg#anchor"),
          GetSubresourceURLForURL(
              GURL("https://www.test.com/public_img.jpg#anchor")),
      },
      {
          GURL("https://www.test.com/"
               "public_img.jpg?public_arg1=bar&public_arg2"),
          GetSubresourceURLForURL(
              GURL("https://www.test.com/"
                   "public_img.jpg?public_arg1=bar&public_arg2")),
      },
      // Private images will not be redirected.
      {
          GURL("https://www.test.com/private_img.jpg"),
          GURL(),
      },
      {
          GURL("https://www.test.com/public_img.jpg&private_arg1=foo"),
          GURL(),
      },
  };
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kSubresourceRedirect,
        {{"enable_lite_page_redirect", "true"}}}},
      {});

  for (const TestCase& test_case : kTestCases) {
    auto throttle = CreateSubresourceRedirectURLLoaderThrottle(
        test_case.original_url, content::ResourceType::kImage,
        content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
        {"https://www.test.com/public_img.jpg",
         "https://www.test.com/public_img.jpg#anchor",
         "https://www.test.com/public_img.jpg?public_arg1=bar&public_arg2"});
    network::ResourceRequest request;
    request.url = test_case.original_url;
    request.resource_type = static_cast<int>(content::ResourceType::kImage);
    request.previews_state = content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON;
    bool defer = true;
    throttle->WillStartRequest(&request, &defer);

    EXPECT_EQ(defer, test_case.redirected_subresource_url.is_empty());
    if (!test_case.redirected_subresource_url.is_empty()) {
      EXPECT_EQ(request.url, test_case.redirected_subresource_url);
    } else {
      EXPECT_EQ(request.url, test_case.original_url);
    }
  }
}

TEST(SubresourceRedirectURLLoaderThrottleTest, DeferOverridenToFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kSubresourceRedirect,
        {{"enable_lite_page_redirect", "true"}}}},
      {});

  auto throttle = CreateSubresourceRedirectURLLoaderThrottle(
      GURL("https://www.test.com/test.jpg"), content::ResourceType::kImage,
      content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
      {"https://www.test.com/test.jpg"});
  network::ResourceRequest request;
  request.url = GURL("https://www.test.com/test.jpg");
  request.resource_type = static_cast<int>(content::ResourceType::kImage);
  request.previews_state = content::PreviewsTypes::SUBRESOURCE_REDIRECT_ON;
  bool defer = true;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
}

}  // namespace
}  // namespace subresource_redirect

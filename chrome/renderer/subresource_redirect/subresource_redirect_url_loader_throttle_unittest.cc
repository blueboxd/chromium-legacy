// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_redirect {

namespace {

TEST(SubresourceRedirectURLLoaderThrottleTest, TestGetSubresourceURL) {
  struct TestCase {
    GURL original_url;
    int resource_type;
    GURL redirected_subresource_url;
  };

  const TestCase kTestCases[]{
      {
          GURL("https://www.test.com/test.jpg"),
          static_cast<int>(content::ResourceType::kImage),
          GetSubresourceURLForURL(GURL("https://www.test.com/test.jpg")),
      },
      {
          GURL("https://www.test.com/test.jpg#test"),
          static_cast<int>(content::ResourceType::kImage),
          GetSubresourceURLForURL(GURL("https://www.test.com/test.jpg#test")),
      },
  };

  for (const TestCase& test_case : kTestCases) {
    network::ResourceRequest request;
    request.url = test_case.original_url;
    request.resource_type = test_case.resource_type;
    bool defer = false;

    SubresourceRedirectURLLoaderThrottle throttle;
    throttle.WillStartRequest(&request, &defer);

    EXPECT_FALSE(defer);
    EXPECT_EQ(request.url, test_case.redirected_subresource_url);
  }
}

TEST(SubresourceRedirectURLLoaderThrottleTest, DeferOverridenToFalse) {
  SubresourceRedirectURLLoaderThrottle throttle;

  network::ResourceRequest request;
  request.url = GURL("https://www.test.com/test.jpg");
  request.resource_type = static_cast<int>(content::ResourceType::kImage);
  bool defer = true;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
}

}  // namespace
}  // namespace subresource_redirect

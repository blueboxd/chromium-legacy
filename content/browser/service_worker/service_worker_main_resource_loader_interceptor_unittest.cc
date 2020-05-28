// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerMainResourceLoaderInterceptorTest : public testing::Test {
 public:
  bool ShouldCreateForNavigation(const GURL& url) {
    return ServiceWorkerMainResourceLoaderInterceptor::
        ShouldCreateForNavigation(url);
  }
};

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_HTTP) {
  EXPECT_TRUE(ShouldCreateForNavigation(GURL("http://host/scope/doc")));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_HTTPS) {
  EXPECT_TRUE(ShouldCreateForNavigation(GURL("https://host/scope/doc")));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_FTP) {
  EXPECT_FALSE(ShouldCreateForNavigation(GURL("ftp://host/scope/doc")));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_ExternalFileScheme) {
  bool expected_handler_created = false;
#if defined(OS_CHROMEOS)
  expected_handler_created = true;
#endif  // OS_CHROMEOS
  EXPECT_EQ(expected_handler_created,
            ShouldCreateForNavigation(GURL("externalfile:drive/doc")));
}

}  // namespace content

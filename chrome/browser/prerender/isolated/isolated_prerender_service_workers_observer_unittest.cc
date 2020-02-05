// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"

#include "base/optional.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

TEST(IsolatedPrerenderServiceWorkersObserverTest, NotReady) {
  const url::Origin kTestOrigin(
      url::Origin::Create(GURL("https://test.com/path?foo=bar")));
  IsolatedPrerenderServiceWorkersObserver observer(nullptr);
  EXPECT_EQ(base::nullopt,
            observer.IsServiceWorkerRegisteredForOrigin(kTestOrigin));
}

TEST(IsolatedPrerenderServiceWorkersObserverTest, UsageInfoCallback) {
  const url::Origin kTestOrigin(
      url::Origin::Create(GURL("https://test.com/path?foo=bar")));
  const url::Origin kOtherOrigin(
      url::Origin::Create(GURL("https://other.com/path?what=ever")));
  IsolatedPrerenderServiceWorkersObserver observer(nullptr);

  content::StorageUsageInfo info{kTestOrigin, /*total_size_bytes=*/0,
                                 /*last_modified=*/base::Time()};
  observer.CallOnHasUsageInfoForTesting({info});

  EXPECT_EQ(base::Optional<bool>(true),
            observer.IsServiceWorkerRegisteredForOrigin(kTestOrigin));
  EXPECT_EQ(base::Optional<bool>(false),
            observer.IsServiceWorkerRegisteredForOrigin(kOtherOrigin));
}

TEST(IsolatedPrerenderServiceWorkersObserverTest, OnRegistration) {
  const GURL kTestURL("https://test.com/path?foo=bar");
  const url::Origin kTestOrigin(url::Origin::Create(kTestURL));
  IsolatedPrerenderServiceWorkersObserver observer(nullptr);
  observer.CallOnHasUsageInfoForTesting({});
  EXPECT_EQ(base::Optional<bool>(false),
            observer.IsServiceWorkerRegisteredForOrigin(kTestOrigin));

  content::ServiceWorkerContextObserver* sw_observer = &observer;
  sw_observer->OnRegistrationCompleted(kTestURL);

  EXPECT_EQ(base::Optional<bool>(true),
            observer.IsServiceWorkerRegisteredForOrigin(kTestOrigin));
}

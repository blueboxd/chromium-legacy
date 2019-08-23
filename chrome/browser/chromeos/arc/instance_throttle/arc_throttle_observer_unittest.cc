// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_throttle_observer.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/template_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcThrottleObserverTest
    : public testing::Test,
      public base::SupportsWeakPtr<ArcThrottleObserverTest> {
 public:
  ArcThrottleObserverTest() {
    observer_.StartObserving(
        nullptr /* ArcBridgeService* */, nullptr /* content::BrowserContext* */,
        base::BindRepeating(&ArcThrottleObserverTest::OnObserverStateChanged,
                            AsWeakPtr()));
  }

  void OnObserverStateChanged() { notify_count_++; }

 protected:
  ArcThrottleObserver* observer() { return &observer_; }
  size_t notify_count() const { return notify_count_; }

 private:
  ArcThrottleObserver observer_{ArcThrottleObserver::PriorityLevel::LOW,
                                "TestObserver"};
  size_t notify_count_{0};

  DISALLOW_COPY_AND_ASSIGN(ArcThrottleObserverTest);
};

// Tests that ArcThrottleObserver can be constructed and destructed.
TEST_F(ArcThrottleObserverTest, TestConstructDestruct) {}

// Tests that ArcThrottleObserver notifies observers only when its 'active'
// state changes
TEST_F(ArcThrottleObserverTest, TestSetActive) {
  EXPECT_EQ(0U, notify_count());
  EXPECT_FALSE(observer()->active());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(1U, notify_count());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(1U, notify_count());

  observer()->SetActive(false);
  EXPECT_FALSE(observer()->active());
  EXPECT_EQ(2U, notify_count());
}

}  // namespace arc

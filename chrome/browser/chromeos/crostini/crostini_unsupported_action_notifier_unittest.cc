// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_unsupported_action_notifier.h"

#include <memory>
#include <tuple>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

using ::testing::_;
using ::testing::Bool;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

class MockDelegate : public CrostiniUnsupportedActionNotifier::Delegate {
 public:
  MOCK_METHOD(bool, IsInTabletMode, (), (override));
  MOCK_METHOD(bool, IsFocusedWindowCrostini, (), (override));
  MOCK_METHOD(void,
              ShowNotification,
              (message_center::Notification * notification),
              (override));
  MOCK_METHOD(void,
              AddFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveFocusObserver,
              (aura::client::FocusChangeObserver * observer),
              (override));
  MOCK_METHOD(void,
              AddTabletModeObserver,
              (ash::TabletModeObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveTabletModeObserver,
              (ash::TabletModeObserver * observer),
              (override));
};

class CrostiniUnsupportedActionNotifierTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  CrostiniUnsupportedActionNotifierTest()
      : notifier(std::make_unique<NiceMock<MockDelegate>>()) {}
  virtual ~CrostiniUnsupportedActionNotifierTest() = default;

  MockDelegate& get_delegate() {
    auto* ptr = notifier.get_delegate_for_testing();
    DCHECK(ptr);

    // Our delegate is always a mock delegate in these tests, but we don't have
    // RTTI so have to use a static cast.
    return static_cast<NiceMock<MockDelegate>&>(*ptr);
  }
  bool is_tablet_mode() const { return std::get<0>(GetParam()); }
  bool is_crostini_focused() const { return std::get<1>(GetParam()); }

  CrostiniUnsupportedActionNotifier notifier;
};

TEST_P(CrostiniUnsupportedActionNotifierTest,
       NotificationShownOnceOnlyWhenEnteringTabletModeWhileCrostiniAppFocused) {
  EXPECT_CALL(get_delegate(), IsInTabletMode)
      .WillRepeatedly(Return(is_tablet_mode()));
  EXPECT_CALL(get_delegate(), IsFocusedWindowCrostini)
      .WillRepeatedly(Return(is_crostini_focused()));
  EXPECT_CALL(get_delegate(), ShowNotification(_))
      .Times((is_tablet_mode() && is_crostini_focused()) ? 1 : 0);

  notifier.OnTabletModeStarted();
  notifier.OnTabletModeStarted();
  notifier.OnTabletModeStarted();
}

TEST_P(CrostiniUnsupportedActionNotifierTest,
       NotificationShownOnceOnlyWhenFocusingCrostiniWhileInTabletMode) {
  EXPECT_CALL(get_delegate(), IsInTabletMode)
      .WillRepeatedly(Return(is_tablet_mode()));
  EXPECT_CALL(get_delegate(), IsFocusedWindowCrostini)
      .WillRepeatedly(Return(is_crostini_focused()));
  EXPECT_CALL(get_delegate(), ShowNotification(_))
      .Times((is_tablet_mode() && is_crostini_focused()) ? 1 : 0);

  notifier.OnWindowFocused(nullptr, nullptr);
  notifier.OnWindowFocused(nullptr, nullptr);
  notifier.OnWindowFocused(nullptr, nullptr);
}

INSTANTIATE_TEST_CASE_P(CrostiniUnsupportedActionNotifierTestCombination,
                        CrostiniUnsupportedActionNotifierTest,
                        ::testing::Combine(Bool(), Bool()));

}  // namespace crostini

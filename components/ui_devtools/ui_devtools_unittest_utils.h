// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_
#define COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ui_devtools {

class FakeFrontendChannel : public protocol::FrontendChannel {
 public:
  FakeFrontendChannel();
  ~FakeFrontendChannel() override;

  int CountProtocolNotificationMessageStartsWith(const std::string& message);

  int CountProtocolNotificationMessage(const std::string& message);

  void SetAllowNotifications(bool allow_notifications) {
    allow_notifications_ = allow_notifications;
  }

  // FrontendChannel:
  void sendProtocolResponse(
      int callId,
      std::unique_ptr<protocol::Serializable> message) override {}
  void flushProtocolNotifications() override {}
  void fallThrough(int call_id,
                   const std::string& method,
                   crdtp::span<uint8_t> message) override {}
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;

 private:
  std::vector<std::string> protocol_notification_messages_;
  bool allow_notifications_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeFrontendChannel);
};

class MockUIElementDelegate : public UIElementDelegate {
 public:
  MockUIElementDelegate();
  ~MockUIElementDelegate() override;

  MOCK_METHOD2(OnUIElementAdded, void(UIElement*, UIElement*));
  MOCK_METHOD2(OnUIElementReordered, void(UIElement*, UIElement*));
  MOCK_METHOD1(OnUIElementRemoved, void(UIElement*));
  MOCK_METHOD1(OnUIElementBoundsChanged, void(UIElement*));
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_

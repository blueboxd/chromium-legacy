// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_SMS_DIALOG_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_SMS_DIALOG_H_

#include "base/macros.h"
#include "content/public/browser/sms_dialog.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSmsDialog : public SmsDialog {
 public:
  MockSmsDialog();
  ~MockSmsDialog() override;

  MOCK_METHOD2(Open, void(RenderFrameHost*, EventHandler handler));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(SmsReceived, void());
  MOCK_METHOD0(SmsTimeout, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsDialog);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_SMS_DIALOG_H_

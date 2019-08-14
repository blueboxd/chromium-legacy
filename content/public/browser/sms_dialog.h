// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMS_DIALOG_H_
#define CONTENT_PUBLIC_BROWSER_SMS_DIALOG_H_

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// SmsDialog controls the dialog that is displayed while the user waits for SMS
// messages.
class CONTENT_EXPORT SmsDialog {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser.sms
  enum Event {
    // User manually clicked the 'Confirm' button.
    kConfirm = 0,
    // User manually dismissed the SMS dialog.
    kCancel = 1,
    // User manually clicked 'Try again' button after a timeout.
    kTimeout = 2,
  };

  using EventHandler = base::OnceCallback<void(Event)>;

  SmsDialog() = default;
  virtual ~SmsDialog() = default;
  virtual void Open(RenderFrameHost* host, EventHandler handler) = 0;
  virtual void Close() = 0;
  virtual void SmsReceived() = 0;
  virtual void SmsTimeout() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsDialog);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMS_DIALOG_H_

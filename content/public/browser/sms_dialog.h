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
  SmsDialog() = default;
  virtual ~SmsDialog() = default;
  virtual void Open(content::RenderFrameHost* host,
                    base::OnceClosure on_continue,
                    base::OnceClosure on_cancel) = 0;
  virtual void Close() = 0;
  virtual void EnableContinueButton() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsDialog);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMS_DIALOG_H_

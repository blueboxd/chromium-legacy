// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMS_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_SMS_SERVICE_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

// The interface to be implemented by the browser to mediate between SMS
// Receiver calls and the underlying operating system.
// https://github.com/samuelgoto/sms-receiver
class CONTENT_EXPORT SmsService {
 public:
  virtual ~SmsService() {}

  static std::unique_ptr<SmsService> Create();

  virtual void Bind(blink::mojom::SmsReceiverRequest request,
                    const url::Origin& origin) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMS_SERVICE_H_

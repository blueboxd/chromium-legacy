// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_SERVICE_H_
#define CONTENT_BROWSER_SMS_SMS_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"

namespace url {
class Origin;
}

namespace content {

// The SmsService is responsible for binding the incoming mojo connections
// from the renderer process and their corresponding SmsReceiverImpl.
// SmsService is owned by BrowserMainLoop, making it effectively a singleton,
// which allows it to serialize and manage a global queue of incoming SMS
// messsages.
class CONTENT_EXPORT SmsService {
 public:
  SmsService();
  ~SmsService();

  void Bind(blink::mojom::SmsReceiverRequest, const url::Origin&);

  // Testing helpers.
  void SetSmsProviderForTest(std::unique_ptr<SmsProvider>);

 private:
  std::unique_ptr<SmsProvider> sms_provider_;

  // Registered clients.
  mojo::StrongBindingSet<blink::mojom::SmsReceiver> bindings_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmsService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_SERVICE_H_

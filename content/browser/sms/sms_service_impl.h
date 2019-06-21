// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SMS_SMS_SERVICE_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/sms_service.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"

namespace url {
class Origin;
}

namespace content {

// The SmsServiceImpl is responsible for taking the incoming mojo calls from the
// renderer process and dispatching them to the SmsProvider platform-specific
// implementation.
class CONTENT_EXPORT SmsServiceImpl : public content::SmsService {
 public:
  SmsServiceImpl();
  ~SmsServiceImpl() override;

  // content::SmsService:
  void Bind(blink::mojom::SmsReceiverRequest, const url::Origin&) override;

  // Testing helpers.
  void SetSmsProviderForTest(std::unique_ptr<SmsProvider>);

 private:
  std::unique_ptr<SmsProvider> sms_provider_;

  // Registered clients.
  mojo::StrongBindingSet<blink::mojom::SmsReceiver> bindings_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmsServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_SERVICE_IMPL_H_

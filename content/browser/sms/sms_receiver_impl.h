// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_
#define CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_

#include <memory>
#include <queue>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT SmsReceiverImpl : public blink::mojom::SmsReceiver,
                                       public content::SmsProvider::Observer {
 public:
  SmsReceiverImpl(SmsProvider*, const url::Origin&);
  ~SmsReceiverImpl() override;

  // content::SmsProvider::Observer:
  bool OnReceive(const url::Origin&, const std::string& message) override;
  void OnTimeout() override;

  // blink::mojom::SmsReceiver:
  void Receive(base::TimeDelta timeout, ReceiveCallback) override;

 private:
  // Manages the queue of callbacks.
  void Push(ReceiveCallback);
  ReceiveCallback Pop();

  // |sms_provider_| is safe because all instances of SmsReceiverImpl are owned
  // by SmsServiceImpl through a StrongBindingSet.
  SmsProvider* sms_provider_;
  const url::Origin origin_;
  std::queue<ReceiveCallback> callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(SmsReceiverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_

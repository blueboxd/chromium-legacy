// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_
#define CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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

  struct Request {
    explicit Request(ReceiveCallback callback);
    ~Request();

    base::OneShotTimer timer;
    ReceiveCallback callback;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // content::SmsProvider::Observer:
  bool OnReceive(const url::Origin&, const std::string& message) override;

  // blink::mojom::SmsReceiver:
  void Receive(base::TimeDelta timeout, ReceiveCallback) override;

 private:
  bool Pop(const std::string& sms);
  void OnTimeout(Request* request);

  // |sms_provider_| is safe because all instances of SmsReceiverImpl are owned
  // by SmsServiceImpl through a StrongBindingSet.
  SmsProvider* sms_provider_;
  const url::Origin origin_;

  using SmsRequestList = std::list<std::unique_ptr<Request>>;
  SmsRequestList requests_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(SmsReceiverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_RECEIVER_IMPL_H_

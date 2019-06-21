// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <queue>
#include <utility>

#include "content/browser/sms/sms_receiver_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace content {

SmsReceiverImpl::SmsReceiverImpl(SmsProvider* sms_provider,
                                 const url::Origin& origin)
    : sms_provider_(sms_provider), origin_(origin) {}

SmsReceiverImpl::~SmsReceiverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SmsReceiverImpl::Receive(base::TimeDelta timeout,
                              ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Push(std::move(callback));
}

void SmsReceiverImpl::Push(ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_.empty())
    sms_provider_->AddObserver(this);

  callbacks_.push(std::move(callback));
  sms_provider_->Retrieve();
}

blink::mojom::SmsReceiver::ReceiveCallback SmsReceiverImpl::Pop() {
  DCHECK(!callbacks_.empty()) << "Unexpected SMS received";

  ReceiveCallback callback = std::move(callbacks_.front());
  callbacks_.pop();

  if (callbacks_.empty())
    sms_provider_->RemoveObserver(this);

  return callback;
}

bool SmsReceiverImpl::OnReceive(const url::Origin& origin,
                                const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_ != origin)
    return false;

  Pop().Run(blink::mojom::SmsStatus::kSuccess, sms);

  return true;
}

void SmsReceiverImpl::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Pop().Run(blink::mojom::SmsStatus::kTimeout, base::nullopt);
}

}  // namespace content

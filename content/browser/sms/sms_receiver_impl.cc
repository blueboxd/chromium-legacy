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

SmsReceiverImpl::Request::Request(ReceiveCallback callback)
    : callback(std::move(callback)) {}

SmsReceiverImpl::Request::~Request() = default;

void SmsReceiverImpl::Receive(base::TimeDelta timeout,
                              ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (requests_.empty())
    sms_provider_->AddObserver(this);

  auto request = std::make_unique<Request>(std::move(callback));
  // The |timer| is owned by |request|, and |request| is owned by |this|, so it
  // is safe to hold raw pointers to |this| and |request| here in the callback.
  request->timer.Start(FROM_HERE, timeout,
                       base::BindOnce(&SmsReceiverImpl::OnTimeout,
                                      base::Unretained(this), request.get()));
  requests_.push_back(std::move(request));
  sms_provider_->Retrieve();
}

bool SmsReceiverImpl::OnReceive(const url::Origin& origin,
                                const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_ != origin)
    return false;

  return Pop(sms);
}

bool SmsReceiverImpl::Pop(const std::string& sms) {
  DCHECK(!requests_.empty());

  DCHECK(requests_.front()->timer.IsRunning());

  requests_.front()->timer.Stop();
  std::move(requests_.front()->callback)
      .Run(blink::mojom::SmsStatus::kSuccess, sms);
  requests_.pop_front();

  if (requests_.empty())
    sms_provider_->RemoveObserver(this);

  return true;
}

void SmsReceiverImpl::OnTimeout(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!request->timer.IsRunning());

  std::move(request->callback)
      .Run(blink::mojom::SmsStatus::kTimeout, base::nullopt);

  // Remove the request from the list.
  for (auto iter = requests_.begin(); iter != requests_.end(); ++iter) {
    if ((*iter).get() == request) {
      requests_.erase(iter);
      if (requests_.empty())
        sms_provider_->RemoveObserver(this);
      return;
    }
  }
}

}  // namespace content

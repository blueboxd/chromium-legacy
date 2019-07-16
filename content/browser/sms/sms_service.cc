// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/sms/sms_receiver_impl.h"

namespace content {

SmsService::SmsService() : sms_provider_(SmsProvider::Create()) {}

SmsService::~SmsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SmsService::Bind(blink::mojom::SmsReceiverRequest request,
                      const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(
      std::make_unique<SmsReceiverImpl>(sms_provider_.get(), origin),
      std::move(request));
}

void SmsService::SetSmsProviderForTest(
    std::unique_ptr<SmsProvider> sms_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sms_provider_ = std::move(sms_provider);
}

}  // namespace content

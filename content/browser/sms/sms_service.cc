// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <iterator>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "content/browser/sms/sms_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using blink::mojom::SmsStatus;

namespace content {

SmsService::SmsService(SmsProvider* provider,
                       const url::Origin& origin,
                       RenderFrameHost* host,
                       blink::mojom::SmsReceiverRequest request)
    : FrameServiceBase(host, std::move(request)),
      sms_provider_(provider),
      origin_(origin) {}

SmsService::SmsService(SmsProvider* provider,
                       RenderFrameHost* host,
                       blink::mojom::SmsReceiverRequest request)
    : SmsService(provider,
                 host->GetLastCommittedOrigin(),
                 host,
                 std::move(request)) {}

SmsService::~SmsService() {
  if (callback_)
    Process(SmsStatus::kTimeout, base::nullopt);
}

// static
void SmsService::Create(SmsProvider* provider,
                        RenderFrameHost* host,
                        blink::mojom::SmsReceiverRequest request) {
  DCHECK(host);

  // SmsService owns itself. It will self-destruct when a mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new SmsService(provider, host, std::move(request));
}

void SmsService::Receive(base::TimeDelta timeout, ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_) {
    std::move(callback).Run(blink::mojom::SmsStatus::kCancelled, base::nullopt);
    return;
  }

  DCHECK(!sms_);

  start_time_ = base::TimeTicks::Now();

  sms_provider_->AddObserver(this);

  callback_ = std::move(callback);
  // The |timer_| is owned by |this|, so it is safe to hold raw pointers to
  // |this| here in the callback.
  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&SmsService::OnTimeout, base::Unretained(this)));

  sms_provider_->Retrieve();
}

bool SmsService::OnReceive(const url::Origin& origin,
                           const std::string& one_time_code,
                           const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_ != origin)
    return false;

  DCHECK(!sms_);
  DCHECK(timer_.IsRunning());
  DCHECK(!start_time_.is_null());

  RecordSmsReceiveTime(base::TimeTicks::Now() - start_time_);

  timer_.Stop();
  sms_provider_->RemoveObserver(this);

  sms_ = sms;
  receive_time_ = base::TimeTicks::Now();

  OpenInfoBar(one_time_code);

  return true;
}

void SmsService::OpenInfoBar(const std::string& one_time_code) {
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());

  web_contents->GetDelegate()->CreateSmsPrompt(
      render_frame_host(), origin_, one_time_code,
      base::BindOnce(&SmsService::OnConfirm, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SmsService::OnCancel, weak_ptr_factory_.GetWeakPtr()));
}

void SmsService::Process(blink::mojom::SmsStatus status,
                         base::Optional<std::string> sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback_);

  std::move(callback_).Run(status, sms);

  CleanUp();
}

void SmsService::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback_);
  DCHECK(!timer_.IsRunning());

  std::move(callback_).Run(blink::mojom::SmsStatus::kTimeout, base::nullopt);

  CleanUp();
}

void SmsService::OnConfirm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(sms_);
  DCHECK(!receive_time_.is_null());
  RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  Process(SmsStatus::kSuccess, sms_);
}

void SmsService::OnCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record only when SMS has already been received.
  DCHECK(!receive_time_.is_null());
  RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  Process(SmsStatus::kCancelled, base::nullopt);
}

void SmsService::CleanUp() {
  timer_.Stop();
  callback_.Reset();
  sms_.reset();
  start_time_ = base::TimeTicks();
  receive_time_ = base::TimeTicks();
  sms_provider_->RemoveObserver(this);
}

}  // namespace content

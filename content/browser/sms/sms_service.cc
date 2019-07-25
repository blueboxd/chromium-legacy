// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <iterator>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "content/public/browser/sms_dialog.h"
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

  DCHECK(!prompt_);
  DCHECK(!sms_);

  sms_provider_->AddObserver(this);

  callback_ = std::move(callback);
  // The |timer_| is owned by |this|, so it is safe to hold raw pointers to
  // |this| here in the callback.
  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&SmsService::OnTimeout, base::Unretained(this)));

  Prompt();

  sms_provider_->Retrieve();
}

bool SmsService::OnReceive(const url::Origin& origin, const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_ != origin)
    return false;

  DCHECK(prompt_);
  DCHECK(!sms_);
  DCHECK(timer_.IsRunning());

  timer_.Stop();
  sms_provider_->RemoveObserver(this);

  sms_ = sms;
  prompt_->EnableContinueButton();
  return true;
}

void SmsService::Process(blink::mojom::SmsStatus status,
                         base::Optional<std::string> sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback_);

  std::move(callback_).Run(status, sms);

  Dismiss();
}

void SmsService::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback_);
  DCHECK(!timer_.IsRunning());

  std::move(callback_).Run(blink::mojom::SmsStatus::kTimeout, base::nullopt);

  Dismiss();
}

void SmsService::OnContinue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(sms_);

  Process(SmsStatus::kSuccess, sms_);
}

void SmsService::OnCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Process(SmsStatus::kCancelled, base::nullopt);
}

void SmsService::Prompt() {
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  prompt_ = web_contents->GetDelegate()->CreateSmsDialog();
  if (prompt_) {
    prompt_->Open(
        render_frame_host(),
        base::BindOnce(&SmsService::OnContinue, base::Unretained(this)),
        base::BindOnce(&SmsService::OnCancel, base::Unretained(this)));
  }
}

void SmsService::Dismiss() {
  if (prompt_) {
    prompt_->Close();
    prompt_.reset();
  }

  timer_.Stop();
  callback_.Reset();
  sms_.reset();
  sms_provider_->RemoveObserver(this);
}

}  // namespace content

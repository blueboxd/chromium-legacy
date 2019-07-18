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
  while (!requests_.empty()) {
    Pop(SmsStatus::kTimeout, base::nullopt);
  }
}

SmsService::Request::Request(ReceiveCallback callback)
    : callback(std::move(callback)) {}

SmsService::Request::~Request() = default;

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
  if (prompt_) {
    std::move(callback).Run(blink::mojom::SmsStatus::kCancelled, base::nullopt);
    return;
  }

  if (requests_.empty())
    sms_provider_->AddObserver(this);

  auto request = std::make_unique<Request>(std::move(callback));
  // The |timer| is owned by |request|, and |request| is owned by |this|, so it
  // is safe to hold raw pointers to |this| and |request| here in the callback.
  request->timer.Start(FROM_HERE, timeout,
                       base::BindOnce(&SmsService::OnTimeout,
                                      base::Unretained(this), request.get()));
  requests_.push_back(std::move(request));

  Prompt();

  sms_provider_->Retrieve();
}

bool SmsService::OnReceive(const url::Origin& origin, const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin_ != origin)
    return false;

  return Pop(SmsStatus::kSuccess, sms);
}

bool SmsService::Pop(blink::mojom::SmsStatus status,
                     base::Optional<std::string> sms) {
  DCHECK(!requests_.empty());

  Dismiss();

  DCHECK(requests_.front()->timer.IsRunning());

  requests_.front()->timer.Stop();
  std::move(requests_.front()->callback).Run(status, sms);
  requests_.pop_front();

  if (requests_.empty())
    sms_provider_->RemoveObserver(this);

  return true;
}

void SmsService::OnTimeout(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!request->timer.IsRunning());

  std::move(request->callback)
      .Run(blink::mojom::SmsStatus::kTimeout, base::nullopt);

  // Remove the request from the list.
  for (auto iter = requests_.begin(); iter != requests_.end(); ++iter) {
    if ((*iter).get() == request) {
      requests_.erase(iter);
      Dismiss();
      if (requests_.empty())
        sms_provider_->RemoveObserver(this);
      return;
    }
  }
}

void SmsService::OnCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Pop(SmsStatus::kCancelled, base::nullopt);
}

void SmsService::Prompt() {
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  prompt_ = web_contents->GetDelegate()->CreateSmsDialog();
  if (prompt_) {
    prompt_->Open(render_frame_host(), base::BindOnce(&SmsService::OnCancel,
                                                      base::Unretained(this)));
  }
}

void SmsService::Dismiss() {
  if (prompt_) {
    prompt_->Close();
    prompt_.reset();
  }
}

}  // namespace content

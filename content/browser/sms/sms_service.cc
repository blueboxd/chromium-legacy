// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <iterator>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "content/browser/sms/sms_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

using blink::SmsReceiverDestroyedReason;
using blink::mojom::SmsStatus;

namespace content {

SmsService::SmsService(
    SmsFetcher* fetcher,
    const url::Origin& origin,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : FrameServiceBase(host, std::move(receiver)),
      fetcher_(fetcher),
      origin_(origin) {}

SmsService::SmsService(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : SmsService(fetcher,
                 host->GetLastCommittedOrigin(),
                 host,
                 std::move(receiver)) {}

SmsService::~SmsService() {
  if (callback_)
    Process(SmsStatus::kTimeout, base::nullopt);
}

// static
void SmsService::Create(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  DCHECK(host);

  // SmsService owns itself. It will self-destruct when a mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new SmsService(fetcher, host, std::move(receiver));
}

void SmsService::Receive(ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This flow relies on the delegate to display an infobar for user
  // confirmation. Cancelling the call early if no delegate is available is
  // easier to debug then silently dropping SMSes later on.
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  if (!web_contents->GetDelegate()) {
    std::move(callback).Run(SmsStatus::kCancelled, base::nullopt);
    return;
  }

  if (callback_) {
    std::move(callback_).Run(SmsStatus::kCancelled, base::nullopt);
    fetcher_->Unsubscribe(origin_, this);
  }

  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);

  // |one_time_code_| and prompt are still present from the previous
  // request so a new subscription is unnecessary.
  if (prompt_open_) {
    // TODO(crbug.com/1024598): Add UMA histogram.
    return;
  }

  fetcher_->Subscribe(origin_, this);
}

void SmsService::OnReceive(const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!one_time_code_);
  DCHECK(!start_time_.is_null());

  RecordSmsReceiveTime(base::TimeTicks::Now() - start_time_);

  one_time_code_ = one_time_code;

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebOtpBackend) !=
      switches::kWebOtpBackendSmsVerification) {
    Process(SmsStatus::kSuccess, one_time_code_);
    return;
  }

  receive_time_ = base::TimeTicks::Now();
  OpenInfoBar(one_time_code);
}

void SmsService::Abort() {
  DCHECK(callback_);
  Process(SmsStatus::kAborted, base::nullopt);
}

void SmsService::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  switch (load_details.type) {
    case NavigationType::NAVIGATION_TYPE_NEW_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateNewPage);
      break;
    case NavigationType::NAVIGATION_TYPE_EXISTING_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateExistingPage);
      break;
    case NavigationType::NAVIGATION_TYPE_SAME_PAGE:
      RecordDestroyedReason(SmsReceiverDestroyedReason::kNavigateSamePage);
      break;
    default:
      // Ignore cases we don't care about.
      break;
  }
}

void SmsService::OpenInfoBar(const std::string& one_time_code) {
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  if (!web_contents->GetDelegate()) {
    Process(SmsStatus::kCancelled, base::nullopt);
    return;
  }

  prompt_open_ = true;
  web_contents->GetDelegate()->CreateSmsPrompt(
      render_frame_host(), origin_, one_time_code,
      base::BindOnce(&SmsService::OnConfirm, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SmsService::OnCancel, weak_ptr_factory_.GetWeakPtr()));
}

void SmsService::Process(blink::mojom::SmsStatus status,
                         base::Optional<std::string> one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_);
  std::move(callback_).Run(status, one_time_code);
  CleanUp();
}

void SmsService::OnConfirm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(one_time_code_);
  DCHECK(!receive_time_.is_null());
  RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  prompt_open_ = false;

  if (!callback_) {
    // Cleanup since request has been aborted while prompt is up.
    CleanUp();
    return;
  }
  Process(SmsStatus::kSuccess, one_time_code_);
}

void SmsService::OnCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record only when SMS has already been received.
  DCHECK(!receive_time_.is_null());
  RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  prompt_open_ = false;

  if (!callback_) {
    // Cleanup since request has been aborted while prompt is up.
    CleanUp();
    return;
  }
  Process(SmsStatus::kCancelled, base::nullopt);
}

void SmsService::CleanUp() {
  // Skip resetting |one_time_code_|, |sms| and |receive_time_| while prompt is
  // still open in case it needs to be returned to the next incoming request
  // upon prompt confirmation.
  if (!prompt_open_) {
    one_time_code_.reset();
    receive_time_ = base::TimeTicks();
  }
  start_time_ = base::TimeTicks();
  callback_.Reset();
  fetcher_->Unsubscribe(origin_, this);
}

}  // namespace content

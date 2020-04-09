// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "base/callback.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

const char kSmsFetcherImplKeyName[] = "sms_fetcher";

SmsFetcherImpl::SmsFetcherImpl(BrowserContext* context,
                               std::unique_ptr<SmsProvider> provider)
    : context_(context), provider_(std::move(provider)) {
  if (provider_)
    provider_->AddObserver(this);
}

SmsFetcherImpl::~SmsFetcherImpl() {
  if (provider_)
    provider_->RemoveObserver(this);
}

// static
SmsFetcher* SmsFetcher::Get(BrowserContext* context) {
  if (!context->GetUserData(kSmsFetcherImplKeyName)) {
    auto fetcher = std::make_unique<SmsFetcherImpl>(context, nullptr);
    context->SetUserData(kSmsFetcherImplKeyName, std::move(fetcher));
  }

  return static_cast<SmsFetcherImpl*>(
      context->GetUserData(kSmsFetcherImplKeyName));
}

SmsFetcher* SmsFetcher::Get(BrowserContext* context,
                            base::WeakPtr<RenderFrameHost> rfh) {
  auto* stored_fetcher = static_cast<SmsFetcherImpl*>(
      context->GetUserData(kSmsFetcherImplKeyName));
  if (!stored_fetcher || !stored_fetcher->CanReceiveSms()) {
    auto fetcher = std::make_unique<SmsFetcherImpl>(
        context, SmsProvider::Create(std::move(rfh)));
    context->SetUserData(kSmsFetcherImplKeyName, std::move(fetcher));
  }
  return static_cast<SmsFetcherImpl*>(
      context->GetUserData(kSmsFetcherImplKeyName));
}

void SmsFetcherImpl::Subscribe(const url::Origin& origin,
                               SmsQueue::Subscriber* subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (subscribers_.HasSubscriber(origin, subscriber))
    return;

  subscribers_.Push(origin, subscriber);

  // Fetches a remote SMS.
  GetContentClient()->browser()->FetchRemoteSms(
      context_, origin,
      base::BindOnce(&SmsFetcherImpl::OnRemote,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetches a local SMS.
  if (provider_)
    provider_->Retrieve();
}

void SmsFetcherImpl::Unsubscribe(const url::Origin& origin,
                                 SmsQueue::Subscriber* subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscribers_.Remove(origin, subscriber);
}

bool SmsFetcherImpl::Notify(const url::Origin& origin,
                            const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* subscriber = subscribers_.Pop(origin);

  if (!subscriber)
    return false;

  subscriber->OnReceive(one_time_code);

  return true;
}

void SmsFetcherImpl::OnRemote(base::Optional<std::string> sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sms)
    return;

  base::Optional<SmsParser::Result> result = SmsParser::Parse(*sms);
  if (!result)
    return;

  Notify(result->origin, result->one_time_code);
}

bool SmsFetcherImpl::OnReceive(const url::Origin& origin,
                               const std::string& one_time_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Notify(origin, one_time_code);
}

bool SmsFetcherImpl::HasSubscribers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return subscribers_.HasSubscribers();
}

bool SmsFetcherImpl::CanReceiveSms() {
  return provider_ != nullptr;
}

void SmsFetcherImpl::SetSmsProviderForTesting(
    std::unique_ptr<SmsProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  provider_ = std::move(provider);
  provider_->AddObserver(this);
}

}  // namespace content

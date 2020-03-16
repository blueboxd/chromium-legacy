// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/sms_client_impl.h"

#include "base/optional.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace autofill {

SmsClientImpl::SmsClientImpl(content::WebContents* web_contents)
    : fetcher_(content::SmsFetcher::Get(web_contents->GetBrowserContext())),
      origin_(url::Origin::Create(web_contents->GetLastCommittedURL())) {}

SmsClientImpl::~SmsClientImpl() {
  fetcher_->Unsubscribe(origin_, this);
}

void SmsClientImpl::Subscribe() {
  fetcher_->Subscribe(origin_, this);
}

void SmsClientImpl::OnReceive(const std::string& one_time_code) {
  one_time_code_ = one_time_code;
}

const std::string& SmsClientImpl::GetOTP() const {
  return one_time_code_;
}

content::SmsFetcher* SmsClientImpl::GetFetcherForTesting() {
  return fetcher_;
}

}  // namespace autofill

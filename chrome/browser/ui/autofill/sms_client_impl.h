// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SMS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SMS_CLIENT_IMPL_H_

#include <string>

#include "components/autofill/core/browser/sms_client.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace autofill {

// SmsClient implementation that receives actual SMSes with one time passwords.
class SmsClientImpl : public SmsClient, public content::SmsFetcher::Subscriber {
 public:
  explicit SmsClientImpl(content::WebContents* web_contents);
  ~SmsClientImpl() override;

  SmsClientImpl(const SmsClientImpl*) = delete;
  SmsClientImpl& operator=(const SmsClientImpl&) = delete;

  // autofill::SmsClient
  void Subscribe() override;
  const std::string& GetOTP() const override;

  // content::SmsFetcher::Subscriber
  void OnReceive(const std::string& one_time_code) override;

  content::SmsFetcher* GetFetcherForTesting();

 private:
  content::SmsFetcher* fetcher_;
  const url::Origin origin_;
  std::string one_time_code_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SMS_CLIENT_IMPL_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/sms_client_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
namespace {

const char kTestUrl[] = "https://www.google.com";

class SmsClientImplTest : public ChromeRenderViewHostTestHarness {
 public:
  SmsClientImplTest() = default;
  ~SmsClientImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kTestUrl));
    sms_client_.reset(new SmsClientImpl(web_contents()));
  }

  void TearDown() override {
    sms_client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SmsClientImpl* sms_client() { return sms_client_.get(); }

 private:
  std::unique_ptr<SmsClientImpl> sms_client_;
};

TEST_F(SmsClientImplTest, SubscribeToReceiveSms) {
  sms_client()->Subscribe();

  content::SmsFetcher* fetcher = sms_client()->GetFetcherForTesting();
  EXPECT_TRUE(fetcher->HasSubscribers());
}

TEST_F(SmsClientImplTest, SaveOtpOnReceive) {
  sms_client()->Subscribe();

  std::string otp = "123";
  sms_client()->OnReceive(otp);

  EXPECT_EQ(sms_client()->GetOTP(), otp);
}

}  // namespace

}  // namespace autofill

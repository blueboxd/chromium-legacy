// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SMS_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SMS_CLIENT_H_

#include <string>

#include "components/autofill/core/browser/sms_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockSmsClient : public SmsClient {
 public:
  MockSmsClient();
  ~MockSmsClient();

  MOCK_METHOD0(Subscribe, void());
  MOCK_CONST_METHOD0(GetOTP, std::string&());
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SMS_CLIENT_H_

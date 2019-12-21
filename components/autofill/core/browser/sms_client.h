// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SMS_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SMS_CLIENT_H_

namespace autofill {

// Interface for subscribing to incoming SMSes with a one time password.
//
// SmsClient will receive SMS messages that follow the server-side convention
// of the SMS Receiver (https://github.com/samuelgoto/sms-receiver).
class SmsClient {
 public:
  virtual ~SmsClient() = default;

  virtual void Subscribe() = 0;

  // Retrieves the one time passcode that has been received in an incoming
  // SMS. If an SMS hasn't been retrieved or a one time passcode has not been
  // obtained, it will return an empty string.
  virtual const std::string& GetOTP() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SMS_CLIENT_H_

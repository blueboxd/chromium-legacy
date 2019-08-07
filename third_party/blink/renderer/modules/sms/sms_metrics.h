// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_

#include <stdint.h>

namespace base {
class TimeDelta;
}

namespace blink {

// Don't change the meaning of these values because they are being recorded in a
// metric.
enum class SMSReceiverOutcome {
  kSuccess = 0,
  kTimeout = 1,
  kConnectionError = 2,
  kCancelled = 3,
  kMaxValue = kCancelled
};

// Records the result of a call to the SMSReceiver API.
void RecordSMSOutcome(SMSReceiverOutcome outcome);

// Records the time from when the API is called to when the user successfully
// receives the SMS and presses continue to move on with the verification flow.
void RecordSMSSuccessTime(base::TimeDelta duration);

// Records the timeout value specified with the API is called. The value of 0
// indicates that no value was specified.
void RecordSMSRequestedTimeout(uint32_t timeout_seconds);

// Records the time from when the API is called to when the user gets timed out.
void RecordSMSTimeoutExceededTime(base::TimeDelta duration);

// Records the time from when the API is called to when the user presses the
// cancel button to abort SMS retrieval.
void RecordSMSCancelTime(base::TimeDelta duration);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_

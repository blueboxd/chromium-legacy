// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_

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

void RecordSMSOutcome(SMSReceiverOutcome outcome);

void RecordSMSSuccessTime(base::TimeDelta duration);

void RecordSMSTimeoutExceededTime(base::TimeDelta duration);

void RecordSMSCancelTime(base::TimeDelta duration);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_

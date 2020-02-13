// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_METRICS_H_

#include <stdint.h>

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/sms/sms_receiver_outcome.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

// Records the result of a call to navigator.credentials.get({otp}) using
// the same histogram as SmsReceiver API to provide continuity with previous
// iterations of the API.
void RecordSmsOutcome(SMSReceiverOutcome outcome,
                      ukm::SourceId source_id,
                      ukm::UkmRecorder* ukm_recorder);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_METRICS_H_

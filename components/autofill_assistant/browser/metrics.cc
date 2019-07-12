// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill_assistant {

namespace {
const char kDropOutEnumName[] = "Android.AutofillAssistant.DropOutReason";
const char kPaymentRequestPrefilledName[] =
    "Android.AutofillAssistant.PaymentRequest.Prefilled";
}  // namespace

// static
void Metrics::RecordDropOut(DropOutReason reason) {
  DCHECK_LE(reason, DropOutReason::NUM_ENTRIES);
  base::UmaHistogramEnumeration(kDropOutEnumName, reason,
                                DropOutReason::NUM_ENTRIES);
}

// static
void Metrics::RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success) {
  if (initially_complete && success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_SUCCESS,
                                  PaymentRequestPrefilled::NUM_ENTRIES);
  } else if (initially_complete && !success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_FAILURE,
                                  PaymentRequestPrefilled::NUM_ENTRIES);
  } else if (!initially_complete && success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::NOTPREFILLED_SUCCESS,
                                  PaymentRequestPrefilled::NUM_ENTRIES);
  } else if (!initially_complete && !success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::NOTPREFILLED_FAILURE,
                                  PaymentRequestPrefilled::NUM_ENTRIES);
  }
}

}  // namespace autofill_assistant

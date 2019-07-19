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
const char kPaymentRequestAutofillInfoChangedName[] =
    "Android.AutofillAssistant.PaymentRequest.AutofillChanged";
}  // namespace

// static
void Metrics::RecordDropOut(DropOutReason reason) {
  DCHECK_LE(reason, DropOutReason::kMaxValue);
  DVLOG_IF(3, reason != DropOutReason::AA_START)
      << "Drop out with reason: " << reason;
  base::UmaHistogramEnumeration(kDropOutEnumName, reason);
}

// static
void Metrics::RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success) {
  if (initially_complete && success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_SUCCESS);
  } else if (initially_complete && !success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_FAILURE);
  } else if (!initially_complete && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestPrefilledName,
        PaymentRequestPrefilled::NOTPREFILLED_SUCCESS);
  } else if (!initially_complete && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestPrefilledName,
        PaymentRequestPrefilled::NOTPREFILLED_FAILURE);
  }
}

// static
void Metrics::RecordPaymentRequestAutofillChanged(bool changed, bool success) {
  if (changed && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::CHANGED_SUCCESS);
  } else if (changed && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::CHANGED_FAILURE);
  } else if (!changed && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::NOTCHANGED_SUCCESS);
  } else if (!changed && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::NOTCHANGED_FAILURE);
  }
}

}  // namespace autofill_assistant

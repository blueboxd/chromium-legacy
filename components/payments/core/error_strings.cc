// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_strings.h"

namespace payments {
namespace errors {

// Please keep the list alphabetized.
// Each string must be on a single line to correctly generate ErrorStrings.java.

const char kAnotherUiShowing[] = "Another PaymentRequest UI is already showing in a different tab or window.";
const char kAttemptedInitializationTwice[] = "Attempted initialization twice.";
const char kCannotAbortWithoutInit[] = "Attempted abort without initialization.";
const char kCannotAbortWithoutShow[] = "Attempted abort without show.";
const char kCannotCallCanMakePaymentWithoutInit[] = "Attempted canMakePayment without initialization.";
const char kCannotCallHasEnrolledInstrumentWithoutInit[] = "Attempted hasEnrolledInstrument without initialization.";
const char kCannotCompleteWithoutInit[] = "Attempted complete without initialization.";
const char kCannotCompleteWithoutShow[] = "Attempted complete without show.";
const char kCannotRetryWithoutInit[] = "Attempted retry without initialization.";
const char kCannotRetryWithoutShow[] = "Attempted retry without show.";
const char kCannotShowInBackgroundTab[] = "Cannot show PaymentRequest UI in a background tab.";
const char kCannotShowTwice[] = "Attempted show twice.";
const char kCannotShowWithoutInit[] = "Attempted show without initialization.";
const char kCannotUpdateWithoutInit[] = "Attempted updateWith without initialization.";
const char kCannotUpdateWithoutShow[] = "Attempted updateWith without show.";
const char kDetailedInvalidSslCertificateMessageFormat[] = "SSL certificate is not valid. Security level: $.";
const char kGenericPaymentMethodNotSupportedMessage[] = "Payment method not supported.";
const char kInvalidSslCertificate[] = "SSL certificate is not valid.";
const char kInvalidState[] = "Invalid state.";
const char kMethodDataRequired[] = "Method data required.";
const char kMethodNameRequired[] = "Method name required.";
const char kMultiplePaymentMethodsNotSupportedFormat[] = "The payment methods $ are not supported.";
const char kNotInASecureOrigin[] = "Not in a secure origin.";
const char kNotInitialized[] = "Not initialized.";
const char kNotShown[] = "Not shown.";
const char kProhibitedOrigin[] = "Only localhost, file://, and cryptographic scheme origins allowed.";
const char kProhibitedOriginOrInvalidSslExplanation[] = "No UI will be shown. CanMakePayment and hasEnrolledInstrument will always return false. Show will be rejected with NotSupportedError.";
const char kSinglePaymentMethodNotSupportedFormat[] = "The payment method $ is not supported.";
const char kTotalRequired[] = "Total required.";
const char kUserCancelled[] = "User closed the Payment Request UI.";

}  // namespace errors
}  // namespace payments

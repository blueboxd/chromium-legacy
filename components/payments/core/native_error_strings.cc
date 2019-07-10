// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/native_error_strings.h"

namespace payments {
namespace errors {

const char kCannotAbortWithoutInit[] =
    "Attempted abort without initialization.";

const char kCannotAbortWithoutShow[] = "Attempted abort without show.";

const char kCannotCallCanMakePaymentWithoutInit[] =
    "Attempted canMakePayment without initialization.";

const char kCannotCallHasEnrolledInstrumentWithoutInit[] =
    "Attempted hasEnrolledInstrument without initialization.";

const char kCannotCompleteWithoutInit[] =
    "Attempted complete without initialization.";

const char kCannotCompleteWithoutShow[] = "Attempted complete without show.";

const char kCannotRetryWithoutInit[] =
    "Attempted retry without initialization.";

const char kCannotRetryWithoutShow[] = "Attempted retry without show.";

const char kDetailedInvalidSslCertificateMessageFormat[] =
    "SSL certificate is not valid. Security level: $.";

const char kInvalidSslCertificate[] = "SSL certificate is not valid.";

const char kMethodDataRequired[] = "Method data required.";

const char kMethodNameRequired[] = "Method name required.";

const char kMissingDetailsFromPaymentApp[] =
    "Payment app returned invalid response. Missing field \"details\".";

const char kMissingMethodNameFromPaymentApp[] =
    "Payment app returned invalid response. Missing field \"methodName\".";

const char kMultiplePaymentMethodsNotSupportedFormat[] =
    "The payment methods $ are not supported.";

const char kNoResponseToPaymentEvent[] =
    "Payment handler did not respond to \"paymentrequest\" event.";

const char kNotInitialized[] = "Not initialized.";

const char kNotShown[] = "Not shown.";

const char kPaymentDetailsNotObject[] =
    "Payment app returned invalid response. \"details\" field is not a "
    "dictionary.";

const char kPaymentDetailsStringifyError[] =
    "Payment app returned invalid response. Unable to JSON-serialize "
    "\"details\".";

const char kPaymentEventBrowserError[] =
    "Browser encountered an error when firing the \"paymentrequest\" event in "
    "the payment handler.";

const char kPaymentEventInternalError[] =
    "Payment handler encountered an internal error when handling the "
    "\"paymentrequest\" event.";

const char kPaymentEventRejected[] =
    "Payment handler rejected the promise passed into "
    "PaymentRequestEvent.respondWith(). This is how payment handlers close "
    "their own window programmatically.";

const char kPaymentEventServiceWorkerError[] =
    "Payment handler failed to provide a response because either the "
    "\"paymentrequest\" event took too long or the service worker stopped for "
    "some reason or was killed before the request finished.";

const char kPaymentEventTimeout[] =
    "The \"paymentrequest\" event timed out after 5 minutes.";

const char kPaymentHandlerInsecureNavigation[] =
    "The payment handler navigated to a page with insecure context, invalid "
    "certificate state, or malicious content.";

const char kSinglePaymentMethodNotSupportedFormat[] =
    "The payment method $ is not supported.";

}  // namespace errors
}  // namespace payments

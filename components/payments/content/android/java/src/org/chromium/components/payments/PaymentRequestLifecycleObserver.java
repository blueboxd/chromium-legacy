// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentValidationErrors;

/** Observe the lifecycle of the PaymentRequest. */
public interface PaymentRequestLifecycleObserver {
    /**
     * Called when all of the PaymentRequest parameters have been initiated and validated.
     * @param params The parameters.
     */
    void onPaymentRequestParamsInitiated(PaymentRequestParams params);

    /**
     * Called after {@link PaymentRequest#retry} is invoked.
     * @param errors The payment validation errors.
     */
    void onRetry(PaymentValidationErrors errors);
}

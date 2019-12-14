// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

/** Interface for factories that create payment apps. */
public interface PaymentAppFactoryInterface {
    /**
     * Creates payment apps for the |delegate|. When this method is invoked, each factory must:
     * 1) Call delegate.onCanMakePaymentCalculated(canMakePayment) exactly once.
     * 2) Filter available apps based on delegate.getMethodData().
     * 3) Call delegate.onPaymentAppCreated(app) for apps that match the method data.
     * 4) Call delegate.onDoneCreatingPaymentApps(this) exactly once.
     *
     * @param delegate Provides information about payment request and receives a list of payment
     * apps.
     */
    void create(PaymentAppFactoryDelegate delegate);
}

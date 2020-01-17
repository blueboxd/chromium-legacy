// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Creates payment apps. */
public class PaymentAppService implements PaymentAppFactoryInterface {
    private static PaymentAppService sInstance;
    List<PaymentAppFactoryInterface> mFactories = new ArrayList<>();

    /** @return The singleton instance of this class. */
    public static PaymentAppService getInstance() {
        if (sInstance == null) sInstance = new PaymentAppService();
        return sInstance;
    }

    /** Prevent instantiation. */
    private PaymentAppService() {
        mFactories.add(PaymentAppFactory.getInstance());
        mFactories.add(new AutofillPaymentAppFactory());
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        Collector collector = new Collector(new HashSet<>(mFactories), delegate);
        int numberOfFactories = mFactories.size();
        for (int i = 0; i < numberOfFactories; i++) {
            mFactories.get(i).create(/*delegate=*/collector);
        }
    }

    /**
     * Collects payment apps from multiple factories and invokes
     * delegate.onDoneCreatingPaymentApps() and delegate.onCanMakePaymentCalculated() only once.
     */
    private final class Collector implements PaymentAppFactoryDelegate {
        private final Set<PaymentAppFactoryInterface> mPendingFactories;
        private final PaymentAppFactoryDelegate mDelegate;

        /** Whether at least one payment app factory has calculated canMakePayment to be true. */
        private boolean mCanMakePayment;

        private Collector(
                Set<PaymentAppFactoryInterface> pendingTasks, PaymentAppFactoryDelegate delegate) {
            mPendingFactories = pendingTasks;
            mDelegate = delegate;
        }

        @Override
        public PaymentAppFactoryParams getParams() {
            return mDelegate.getParams();
        }

        @Override
        public void onCanMakePaymentCalculated(boolean canMakePayment) {
            // If all payment app factories return false for canMakePayment, then
            // onCanMakePaymentCalculated(false) is called finally in
            // onDoneCreatingPaymentApps(factory).
            if (!canMakePayment || mCanMakePayment) return;
            mCanMakePayment = true;
            mDelegate.onCanMakePaymentCalculated(true);
        }

        @Override
        public void onAutofillPaymentAppCreatorAvailable(AutofillPaymentAppCreator creator) {
            mDelegate.onAutofillPaymentAppCreatorAvailable(creator);
        }

        @Override
        public void onPaymentAppCreated(PaymentInstrument paymentApp) {
            mDelegate.onPaymentAppCreated(paymentApp);
        }

        @Override
        public void onPaymentAppCreationError(String errorMessage) {
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        @Override
        public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {
            mPendingFactories.remove(factory);
            if (mPendingFactories.isEmpty()) {
                if (!mCanMakePayment) mDelegate.onCanMakePaymentCalculated(false);
                mDelegate.onDoneCreatingPaymentApps(PaymentAppService.this);
            }
        }
    }
}

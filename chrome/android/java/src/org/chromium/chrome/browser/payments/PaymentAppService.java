// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            mFactories.add(new ServiceWorkerPaymentAppBridge());
        }
    }

    /** @param factory The factory to add. */
    public void addFactory(PaymentAppFactoryInterface factory) {
        mFactories.add(factory);
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
        private final List<PaymentInstrument> mPossiblyDuplicatePaymentApps = new ArrayList<>();
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
            mPossiblyDuplicatePaymentApps.add(paymentApp);
        }

        @Override
        public void onPaymentAppCreationError(String errorMessage) {
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        @Override
        public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {
            mPendingFactories.remove(factory);
            if (!mPendingFactories.isEmpty()) return;

            if (!mCanMakePayment) mDelegate.onCanMakePaymentCalculated(false);

            Set<PaymentInstrument> uniquePaymentApps =
                    deduplicatePaymentApps(mPossiblyDuplicatePaymentApps);
            mPossiblyDuplicatePaymentApps.clear();

            for (PaymentInstrument app : uniquePaymentApps) {
                mDelegate.onPaymentAppCreated(app);
            }

            mDelegate.onDoneCreatingPaymentApps(PaymentAppService.this);
        }
    }

    private static Set<PaymentInstrument> deduplicatePaymentApps(List<PaymentInstrument> apps) {
        Map<String, PaymentInstrument> identifierToAppMapping = new HashMap<>();
        int numberOfApps = apps.size();
        for (int i = 0; i < numberOfApps; i++) {
            identifierToAppMapping.put(apps.get(i).getIdentifier(), apps.get(i));
        }

        for (int i = 0; i < numberOfApps; i++) {
            // Used by built-in native payment apps (such as Google Pay) to hide the service worker
            // based payment handler that should be used only on desktop.
            identifierToAppMapping.remove(apps.get(i).getApplicationIdentifierToHide());
        }

        Set<PaymentInstrument> uniquePaymentApps = new HashSet<>(identifierToAppMapping.values());
        for (PaymentInstrument app : identifierToAppMapping.values()) {
            // The list of native applications from the web app manifest's "related_applications"
            // section. If "prefer_related_applications" is true in the manifest and any one of the
            // related application is installed on the device, then the corresponding service worker
            // will be hidden.
            Set<String> identifiersOfAppsThatHidesThisApp =
                    app.getApplicationIdentifiersThatHideThisApp();
            if (identifiersOfAppsThatHidesThisApp == null) continue;
            for (String identifier : identifiersOfAppsThatHidesThisApp) {
                if (identifierToAppMapping.containsKey(identifier)) uniquePaymentApps.remove(app);
            }
        }

        return uniquePaymentApps;
    }
}

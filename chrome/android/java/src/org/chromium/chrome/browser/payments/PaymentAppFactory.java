// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.v4.util.ArrayMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.PaymentApp.InstrumentsCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Builds instances of payment apps.
 */
public class PaymentAppFactory implements PaymentAppFactoryInterface {
    private static PaymentAppFactory sInstance;

    /**
     * Can be used to build additional types of payment apps without Chrome knowing about their
     * types.
     */
    private final List<PaymentAppFactoryAddition> mAdditionalFactories;

    /**
     * Interface for receiving newly created apps.
     */
    public interface PaymentAppCreatedCallback {
        /**
         * Called when the factory has create a payment app. This method may be called
         * zero, one, or many times before the app creation is finished.
         */
        void onPaymentAppCreated(PaymentApp paymentApp);

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         */
        void onGetPaymentAppsError(String errorMessage);

        /**
         * Called when the factory is finished creating payment apps.
         */
        void onAllPaymentAppsCreated();
    }

    /**
     * The interface for additional payment app factories.
     */
    public interface PaymentAppFactoryAddition {
        /**
         * Builds instances of payment apps.
         *
         * @param webContents The web contents that invoked PaymentRequest.
         * @param methodData  The methods that the merchant supports, along with the method specific
         *                    data.
         * @param mayCrawl    Whether crawling for installable payment apps is allowed.
         * @param callback    The callback to invoke when apps are created.
         */
        void create(WebContents webContents, Map<String, PaymentMethodData> methodData,
                boolean mayCrawl, PaymentAppCreatedCallback callback);
    }

    private PaymentAppFactory() {
        mAdditionalFactories = new ArrayList<>();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_PAYMENT_APPS)) {
            mAdditionalFactories.add(new AndroidPaymentAppFactory());
        }
    }

    /**
     * @return The singleton PaymentAppFactory instance.
     */
    public static PaymentAppFactory getInstance() {
        if (sInstance == null) sInstance = new PaymentAppFactory();
        return sInstance;
    }

    /**
     * Add an additional factory that can build instances of payment apps.
     *
     * @param additionalFactory Can build instances of payment apps.
     */
    @VisibleForTesting
    public void addAdditionalFactory(PaymentAppFactoryAddition additionalFactory) {
        mAdditionalFactories.add(additionalFactory);
    }

    /**
     * Builds instances of payment apps.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     * @param methodData  The methods that the merchant supports, along with the method specific
     *                    data.
     * @param mayCrawl    Whether crawling for installable payment apps is allowed.
     * @param callback    The callback to invoke when apps are created.
     */
    public void create(WebContents webContents, Map<String, PaymentMethodData> methodData,
            boolean mayCrawl, final PaymentAppCreatedCallback callback) {
        if (mAdditionalFactories.isEmpty()) {
            callback.onAllPaymentAppsCreated();
            return;
        }

        final Set<PaymentAppFactoryAddition> mPendingTasks = new HashSet<>(mAdditionalFactories);

        for (int i = 0; i < mAdditionalFactories.size(); i++) {
            final PaymentAppFactoryAddition additionalFactory = mAdditionalFactories.get(i);
            PaymentAppCreatedCallback cb = new PaymentAppCreatedCallback() {
                @Override
                public void onPaymentAppCreated(PaymentApp paymentApp) {
                    callback.onPaymentAppCreated(paymentApp);
                }

                @Override
                public void onGetPaymentAppsError(String errorMessage) {
                    callback.onGetPaymentAppsError(errorMessage);
                }

                @Override
                public void onAllPaymentAppsCreated() {
                    mPendingTasks.remove(additionalFactory);
                    if (mPendingTasks.isEmpty()) callback.onAllPaymentAppsCreated();
                }
            };
            additionalFactory.create(webContents, methodData, mayCrawl, cb);
        }
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        create(delegate.getParams().getWebContents(), delegate.getParams().getMethodData(),
                delegate.getParams().getMayCrawl(), /*callback=*/new Aggregator(delegate));
    }

    /** Collects, filters, and returns payment apps to the PaymentAppFactoryDelegate. */
    private final class Aggregator implements PaymentAppCreatedCallback, InstrumentsCallback {
        private final PaymentAppFactoryDelegate mDelegate;
        private final List<PaymentApp> mApps = new ArrayList<>();
        private List<PaymentApp> mPendingApps;

        private Aggregator(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        // PaymentAppCreatedCallback implementation.
        @Override
        public void onPaymentAppCreated(PaymentApp paymentApp) {
            mApps.add(paymentApp);
        }

        // PaymentAppCreatedCallback implementation.
        @Override
        public void onGetPaymentAppsError(String errorMessage) {
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        // PaymentAppCreatedCallback implementation.
        @Override
        public void onAllPaymentAppsCreated() {
            assert mPendingApps == null;

            mPendingApps = new ArrayList<>(mApps);

            Map<PaymentApp, Map<String, PaymentMethodData>> queryApps = new ArrayMap<>();
            for (int i = 0; i < mApps.size(); i++) {
                PaymentApp app = mApps.get(i);
                Map<String, PaymentMethodData> appMethods = filterMerchantMethodData(
                        mDelegate.getParams().getMethodData(), app.getAppMethodNames());
                if (appMethods == null || !app.supportsMethodsAndData(appMethods)) {
                    mPendingApps.remove(app);
                } else {
                    queryApps.put(app, appMethods);
                }
            }

            mDelegate.onCanMakePaymentCalculated(!queryApps.isEmpty());

            if (queryApps.isEmpty()) {
                mDelegate.onDoneCreatingPaymentApps(PaymentAppFactory.this);
                return;
            }

            for (Map.Entry<PaymentApp, Map<String, PaymentMethodData>> q : queryApps.entrySet()) {
                PaymentApp app = q.getKey();
                Map<String, PaymentMethodData> paymentMethods = q.getValue();
                app.setPaymentRequestUpdateEventCallback(
                        mDelegate.getParams().getPaymentRequestUpdateEventCallback());
                app.getInstruments(mDelegate.getParams().getId(), paymentMethods,
                        mDelegate.getParams().getTopLevelOrigin(),
                        mDelegate.getParams().getPaymentRequestOrigin(),
                        mDelegate.getParams().getCertificateChain(),
                        mDelegate.getParams().getModifiers(), /*callback=*/this);
            }
        }

        /**
         * Filter out merchant method data that's not relevant to a payment app. Can return null.
         */
        private Map<String, PaymentMethodData> filterMerchantMethodData(
                Map<String, PaymentMethodData> merchantMethodData, Set<String> appMethods) {
            Map<String, PaymentMethodData> result = null;
            for (String method : appMethods) {
                if (merchantMethodData.containsKey(method)) {
                    if (result == null) result = new ArrayMap<>();
                    result.put(method, merchantMethodData.get(method));
                }
            }
            return result == null ? null : Collections.unmodifiableMap(result);
        }

        // InstrumentsCallback implementation.
        @Override
        public void onInstrumentsReady(PaymentApp app, List<PaymentInstrument> instruments) {
            mPendingApps.remove(app);

            if (instruments != null) {
                for (int i = 0; i < instruments.size(); i++) {
                    PaymentInstrument instrument = instruments.get(i);
                    Set<String> instrumentMethodNames =
                            new HashSet<>(instrument.getInstrumentMethodNames());
                    instrumentMethodNames.retainAll(mDelegate.getParams().getMethodData().keySet());
                    if (!instrumentMethodNames.isEmpty()) {
                        mDelegate.onPaymentAppCreated(instrument);
                    } else {
                        instrument.dismissInstrument();
                    }
                }
            }

            if (mPendingApps.isEmpty()) mDelegate.onDoneCreatingPaymentApps(PaymentAppFactory.this);
        }
    }
}

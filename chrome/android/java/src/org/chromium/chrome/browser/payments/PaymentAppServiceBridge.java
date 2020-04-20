// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Native bridge for finding payment apps.
 */
public class PaymentAppServiceBridge implements PaymentAppFactoryInterface {
    private static final String TAG = "cr_PaymentAppService";
    private static boolean sCanMakePaymentForTesting;
    private final Set<String> mStandardizedPaymentMethods = new HashSet<>();

    /* package */ PaymentAppServiceBridge() {
        mStandardizedPaymentMethods.add(MethodStrings.BASIC_CARD);
        mStandardizedPaymentMethods.add(MethodStrings.INTERLEDGER);
        mStandardizedPaymentMethods.add(MethodStrings.PAYEE_CREDIT_TRANSFER);
        mStandardizedPaymentMethods.add(MethodStrings.PAYER_CREDIT_TRANSFER);
        mStandardizedPaymentMethods.add(MethodStrings.TOKENIZED_CARD);
    }

    /**
     * Make canMakePayment() return true always for testing purpose.
     *
     * @param canMakePayment Indicates whether a SW payment app can make payment.
     */
    @VisibleForTesting
    public static void setCanMakePaymentForTesting(boolean canMakePayment) {
        sCanMakePaymentForTesting = canMakePayment;
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        assert delegate.getParams().getPaymentRequestOrigin().equals(
                UrlFormatter.formatUrlForSecurityDisplay(
                        delegate.getParams().getRenderFrameHost().getLastCommittedURL()));

        PaymentAppServiceCallback callback = new PaymentAppServiceCallback(delegate);

        ByteBuffer[] serializedMethodData =
                new ByteBuffer[delegate.getParams().getMethodData().values().size()];
        int i = 0;
        for (PaymentMethodData methodData : delegate.getParams().getMethodData().values()) {
            serializedMethodData[i++] = methodData.serialize();
        }
        PaymentAppServiceBridgeJni.get().create(delegate.getParams().getRenderFrameHost(),
                delegate.getParams().getTopLevelOrigin(), serializedMethodData,
                delegate.getParams().getMayCrawl(), callback);
    }

    /** Handles callbacks from native PaymentAppService and creates PaymentApps. */
    public class PaymentAppServiceCallback {
        private final PaymentAppFactoryDelegate mDelegate;
        private boolean mPaymentHandlerWithMatchingMethodFound;
        private int mNumberOfPendingCanMakePaymentEvents;
        private boolean mDoneCreatingPaymentApps;

        private PaymentAppServiceCallback(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        /** Called when an installed payment handler is found. */
        @CalledByNative("PaymentAppServiceCallback")
        private void onInstalledPaymentHandlerFound(long registrationId, GURL scope,
                @Nullable String name, @Nullable String userHint, @Nullable Bitmap icon,
                String[] methodNameArray, boolean explicitlyVerified, Object[] capabilities,
                String[] preferredRelatedApplications, Object supportedDelegations) {
            ThreadUtils.assertOnUiThread();

            WebContents webContents = mDelegate.getParams().getWebContents();
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);

            ServiceWorkerPaymentApp app = createInstalledServiceWorkerPaymentApp(webContents,
                    registrationId, scope, name, userHint, icon, methodNameArray,
                    explicitlyVerified, (ServiceWorkerPaymentApp.Capabilities[]) capabilities,
                    preferredRelatedApplications, (SupportedDelegations) supportedDelegations);
            if (app == null) return;

            mPaymentHandlerWithMatchingMethodFound = true;
            mNumberOfPendingCanMakePaymentEvents++;

            ServiceWorkerPaymentAppBridge.CanMakePaymentEventCallback canMakePaymentEventCallback =
                    new ServiceWorkerPaymentAppBridge.CanMakePaymentEventCallback() {
                        @Override
                        public void onCanMakePaymentEventResponse(String errorMessage,
                                boolean canMakePayment, boolean readyForMinimalUI,
                                @Nullable String accountBalance) {
                            if (canMakePayment) mDelegate.onPaymentAppCreated(app);
                            if (!TextUtils.isEmpty(errorMessage)) {
                                mDelegate.onPaymentAppCreationError(errorMessage);
                            }
                            app.setIsReadyForMinimalUI(readyForMinimalUI);
                            app.setAccountBalance(accountBalance);

                            if (--mNumberOfPendingCanMakePaymentEvents == 0
                                    && mDoneCreatingPaymentApps) {
                                notifyFinished();
                            }
                        }
                    };

            if (sCanMakePaymentForTesting || activity.getCurrentTabModel().isIncognito()
                    || mStandardizedPaymentMethods.containsAll(Arrays.asList(methodNameArray))
                    || !explicitlyVerified) {
                canMakePaymentEventCallback.onCanMakePaymentEventResponse(/*errorMessage=*/null,
                        /*canMakePayment=*/true,
                        /*readyForMinimalUI=*/false, /*accountBalance=*/null);
                return;
            }

            Set<PaymentMethodData> supportedRequestedMethodData = new HashSet<>();
            for (String methodName : methodNameArray) {
                if (mDelegate.getParams().getMethodData().containsKey(methodName)) {
                    supportedRequestedMethodData.add(
                            mDelegate.getParams().getMethodData().get(methodName));
                }
            }

            Set<PaymentDetailsModifier> supportedRequestedModifiers = new HashSet<>();
            for (String methodName : methodNameArray) {
                if (mDelegate.getParams().getModifiers().containsKey(methodName)) {
                    supportedRequestedModifiers.add(
                            mDelegate.getParams().getModifiers().get(methodName));
                }
            }

            ServiceWorkerPaymentAppBridge.fireCanMakePaymentEvent(webContents, registrationId,
                    scope, mDelegate.getParams().getId(), mDelegate.getParams().getTopLevelOrigin(),
                    mDelegate.getParams().getPaymentRequestOrigin(),
                    supportedRequestedMethodData.toArray(new PaymentMethodData[0]),
                    supportedRequestedModifiers.toArray(new PaymentDetailsModifier[0]),
                    ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_MINIMAL_UI)
                            ? mDelegate.getParams().getTotalAmountCurrency()
                            : null,
                    canMakePaymentEventCallback);
        }

        /** Called when an installable payment handler is found. */
        @CalledByNative("PaymentAppServiceCallback")
        private void onInstallablePaymentHandlerFound(@Nullable String name, GURL swUrl, GURL scope,
                boolean useCache, @Nullable Bitmap icon, String methodName,
                String[] preferredRelatedApplications, Object supportedDelegations) {
            ThreadUtils.assertOnUiThread();

            ServiceWorkerPaymentApp installableApp = createInstallableServiceWorkerPaymentApp(
                    mDelegate.getParams().getWebContents(), name, swUrl, scope, useCache, icon,
                    methodName, preferredRelatedApplications,
                    (SupportedDelegations) supportedDelegations);

            if (installableApp == null) return;
            mDelegate.onPaymentAppCreated(installableApp);
            mPaymentHandlerWithMatchingMethodFound = true;
            if (mNumberOfPendingCanMakePaymentEvents == 0 && mDoneCreatingPaymentApps) {
                notifyFinished();
            }
        }

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreationError(String errorMessage) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        /**
         * Called when the factory is finished creating payment apps. Expects to be called exactly
         * once and after all onPaymentAppCreated() calls.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onDoneCreatingPaymentApps() {
            ThreadUtils.assertOnUiThread();
            mDoneCreatingPaymentApps = true;
            if (mNumberOfPendingCanMakePaymentEvents == 0) {
                notifyFinished();
            }
        }

        /**
         * Signal completion of payment app lookup.
         */
        private void notifyFinished() {
            assert mDoneCreatingPaymentApps;
            assert mNumberOfPendingCanMakePaymentEvents == 0;
            mDelegate.onCanMakePaymentCalculated(mPaymentHandlerWithMatchingMethodFound);
            mDelegate.onDoneCreatingPaymentApps(PaymentAppServiceBridge.this);
        }
    }

    @CalledByNative
    private static Object[] createCapabilities(int count) {
        return new ServiceWorkerPaymentApp.Capabilities[count];
    }

    @CalledByNative
    private static void addCapabilities(
            Object[] capabilities, int index, int[] supportedCardNetworks) {
        assert index < capabilities.length;
        capabilities[index] = new ServiceWorkerPaymentApp.Capabilities(supportedCardNetworks);
    }

    @CalledByNative
    private static Object createSupportedDelegations(
            boolean shippingAddress, boolean payerName, boolean payerPhone, boolean payerEmail) {
        return new SupportedDelegations(shippingAddress, payerName, payerPhone, payerEmail);
    }

    private static @Nullable ServiceWorkerPaymentApp createInstalledServiceWorkerPaymentApp(
            WebContents webContents, long registrationId, GURL scope, @Nullable String name,
            @Nullable String userHint, @Nullable Bitmap icon, String[] methodNameArray,
            boolean explicitlyVerified, ServiceWorkerPaymentApp.Capabilities[] capabilities,
            String[] preferredRelatedApplications, SupportedDelegations supportedDelegations) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        if (activity == null) return null;
        if (!UriUtils.isURLValid(scope)) {
            Log.e(TAG, "service worker scope is not a valid URL");
        }

        return new ServiceWorkerPaymentApp(webContents, registrationId, scope, name, userHint,
                icon == null ? null : new BitmapDrawable(activity.getResources(), icon),
                methodNameArray, capabilities, preferredRelatedApplications, supportedDelegations);
    }

    private static @Nullable ServiceWorkerPaymentApp createInstallableServiceWorkerPaymentApp(
            WebContents webContents, @Nullable String name, GURL swUrl, GURL scope,
            boolean useCache, @Nullable Bitmap icon, String methodName,
            String[] preferredRelatedApplications, SupportedDelegations supportedDelegations) {
        Context context = ChromeActivity.fromWebContents(webContents);
        if (context == null) return null;
        if (!UriUtils.isURLValid(swUrl)) {
            Log.e(TAG, "service worker installation url is not a valid URL");
        }
        if (!UriUtils.isURLValid(scope)) {
            Log.e(TAG, "service worker scope is not a valid URL");
        }

        return new ServiceWorkerPaymentApp(webContents, name, swUrl, scope, useCache,
                icon == null ? null : new BitmapDrawable(context.getResources(), icon), methodName,
                preferredRelatedApplications, supportedDelegations);
    }

    @NativeMethods
    /* package */ interface Natives {
        void create(RenderFrameHost initiatorRenderFrameHost, String topOrigin,
                ByteBuffer[] methodData, boolean mayCrawlForInstallablePaymentApps,
                PaymentAppServiceCallback callback);
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.intent.IsReadyToPayServiceHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * The point of interaction with a locally installed 3rd party native Android payment app.
 * https://developers.google.com/web/fundamentals/payments/payment-apps-developer-guide/android-payment-apps
 */
public class AndroidPaymentApp extends PaymentApp
        implements WindowAndroid.IntentCallback, IsReadyToPayServiceHelper.ResultHandler {
    private final Handler mHandler;
    private final WebContents mWebContents;
    private final Set<String> mMethodNames;
    private final boolean mIsIncognito;
    private final String mPackageName;
    private final String mPayActivityName;
    private final String mIsReadyToPayServiceName;
    private IsReadyToPayCallback mIsReadyToPayCallback;
    private InstrumentDetailsCallback mInstrumentDetailsCallback;
    private IsReadyToPayServiceHelper mIsReadyToPayServiceHelper;
    @Nullable
    private String mApplicationIdentifierToHide;
    private boolean mBypassIsReadyToPayServiceInTest;
    private final SupportedDelegations mSupportedDelegations;

    // Set inside launchPaymentApp and used to validate the received response.
    @Nullable
    private WebPaymentIntentHelperType.PaymentOptions mPaymentOptions;

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param webContents The web contents.
     * @param packageName The name of the package of the payment app.
     * @param activity The name of the payment activity in the payment app.
     * @param isReadyToPayService The name of the service that can answer "is ready to pay" query,
     *         or null of none.
     * @param label The UI label to use for the payment app.
     * @param icon The icon to use in UI for the payment app.
     * @param isIncognito Whether the user is in incognito mode.
     * @param appToHide The identifier of the application that this app can hide.
     * @param supportedDelegations Delegations which this app can support.
     */
    public AndroidPaymentApp(WebContents webContents, String packageName, String activity,
            @Nullable String isReadyToPayService, String label, Drawable icon, boolean isIncognito,
            @Nullable String appToHide, SupportedDelegations supportedDelegations) {
        super(packageName, label, null, icon);
        ThreadUtils.assertOnUiThread();
        mHandler = new Handler();
        mWebContents = webContents;

        mPackageName = packageName;
        mPayActivityName = activity;
        mIsReadyToPayServiceName = isReadyToPayService;

        if (mIsReadyToPayServiceName != null) {
            assert !isIncognito;
        }

        mMethodNames = new HashSet<>();
        mIsIncognito = isIncognito;
        mApplicationIdentifierToHide = appToHide;
        mSupportedDelegations = supportedDelegations;
    }

    /** @param methodName A payment method that this app supports, e.g., "https://bobpay.com". */
    public void addMethodName(String methodName) {
        mMethodNames.add(methodName);
    }

    /** Callback for receiving responses to IS_READY_TO_PAY queries. */
    /* package */ interface IsReadyToPayCallback {
        /**
         * Called after it is known whether the given app is ready to pay.
         * @param app          The app that has been queried.
         * @param isReadyToPay Whether the app is ready to pay.
         */
        void onIsReadyToPayResponse(AndroidPaymentApp app, boolean isReadyToPay);
    }

    /** Queries the IS_READY_TO_PAY service. */
    /* package */ void maybeQueryIsReadyToPayService(Map<String, PaymentMethodData> methodDataMap,
            String origin, String iframeOrigin, @Nullable byte[][] certificateChain,
            Map<String, PaymentDetailsModifier> modifiers, IsReadyToPayCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mIsReadyToPayCallback
                == null : "Have not responded to previous IS_READY_TO_PAY request";

        mIsReadyToPayCallback = callback;
        if (mIsReadyToPayServiceName == null) {
            respondToIsReadyToPayQuery(true);
            return;
        }

        assert !mIsIncognito;

        Intent isReadyToPayIntent = WebPaymentIntentHelper.createIsReadyToPayIntent(
                /*packageName=*/mPackageName, /*serviceName=*/mIsReadyToPayServiceName,
                removeUrlScheme(origin), removeUrlScheme(iframeOrigin), certificateChain,
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(methodDataMap));
        if (mBypassIsReadyToPayServiceInTest) {
            respondToIsReadyToPayQuery(true);
            return;
        }
        mIsReadyToPayServiceHelper = new IsReadyToPayServiceHelper(
                ContextUtils.getApplicationContext(), isReadyToPayIntent, /*resultHandler=*/this);
    }

    @VisibleForTesting
    /* package */ void bypassIsReadyToPayServiceInTest() {
        mBypassIsReadyToPayServiceInTest = true;
    }

    private void respondToIsReadyToPayQuery(boolean isReadyToPay) {
        ThreadUtils.assertOnUiThread();
        if (mIsReadyToPayCallback == null) return;
        mIsReadyToPayCallback.onIsReadyToPayResponse(/*app=*/this, isReadyToPay);
        mIsReadyToPayCallback = null;
    }

    @Override
    @Nullable
    public String getApplicationIdentifierToHide() {
        return mApplicationIdentifierToHide;
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public void invokePaymentApp(final String id, final String merchantName, String origin,
            String iframeOrigin, final byte[][] certificateChain,
            final Map<String, PaymentMethodData> methodDataMap, final PaymentItem total,
            final List<PaymentItem> displayItems,
            final Map<String, PaymentDetailsModifier> modifiers,
            final PaymentOptions paymentOptions, final List<PaymentShippingOption> shippingOptions,
            InstrumentDetailsCallback callback) {
        mInstrumentDetailsCallback = callback;

        final String schemelessOrigin = removeUrlScheme(origin);
        final String schemelessIframeOrigin = removeUrlScheme(iframeOrigin);
        if (!mIsIncognito) {
            launchPaymentApp(id, merchantName, schemelessOrigin, schemelessIframeOrigin,
                    certificateChain, methodDataMap, total, displayItems, modifiers, paymentOptions,
                    shippingOptions);
            return;
        }

        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        if (activity == null) {
            notifyErrorInvokingPaymentApp(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        new UiUtils.CompatibleAlertDialogBuilder(activity, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_payment_app_leave_incognito_warning)
                .setPositiveButton(R.string.ok,
                        (OnClickListener) (dialog, which)
                                -> launchPaymentApp(id, merchantName, schemelessOrigin,
                                        schemelessIframeOrigin, certificateChain, methodDataMap,
                                        total, displayItems, modifiers, paymentOptions,
                                        shippingOptions))
                .setNegativeButton(R.string.cancel,
                        (OnClickListener) (dialog, which)
                                -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .setOnCancelListener(
                        dialog -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .show();
    }

    @Override
    public boolean handlesShippingAddress() {
        return mSupportedDelegations.getShippingAddress();
    }

    @Override
    public boolean handlesPayerName() {
        return mSupportedDelegations.getPayerName();
    }

    @Override
    public boolean handlesPayerEmail() {
        return mSupportedDelegations.getPayerEmail();
    }

    @Override
    public boolean handlesPayerPhone() {
        return mSupportedDelegations.getPayerPhone();
    }

    private static String removeUrlScheme(String url) {
        return UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }

    private void launchPaymentApp(String id, String merchantName, String origin,
            String iframeOrigin, byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap, PaymentItem total,
            List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers,
            PaymentOptions paymentOptions, List<PaymentShippingOption> shippingOptions) {
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mInstrumentDetailsCallback != null;
        mPaymentOptions =
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentOptions(paymentOptions);

        if (mWebContents.isDestroyed()) {
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window == null) {
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        Intent payIntent = WebPaymentIntentHelper.createPayIntent(mPackageName, mPayActivityName,
                id, merchantName, origin, iframeOrigin, certificateChain,
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(methodDataMap),
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentItem(total),
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentItems(displayItems),
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentDetailsModifierMap(modifiers),
                mPaymentOptions,
                WebPaymentIntentHelperTypeConverter.fromMojoShippingOptionList(shippingOptions));
        try {
            if (!window.showIntent(payIntent, this, R.string.payments_android_app_error)) {
                notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            }
        } catch (SecurityException e) {
            // Payment app does not have android:exported="true" on the PAY activity.
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_PRIVATE_ACTIVITY);
        }
    }

    private void notifyErrorInvokingPaymentApp(String errorMessage) {
        mHandler.post(() -> mInstrumentDetailsCallback.onInstrumentDetailsError(errorMessage));
    }

    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode, Intent data) {
        ThreadUtils.assertOnUiThread();
        window.removeIntentCallback(this);
        WebPaymentIntentHelper.parsePaymentResponse(resultCode, data, mPaymentOptions,
                (errorString)
                        -> notifyErrorInvokingPaymentApp(errorString),
                (methodName, details, payerData)
                        -> mInstrumentDetailsCallback.onInstrumentDetailsReady(
                                methodName, details, payerData));
        mInstrumentDetailsCallback = null;
    }

    @Override
    public void dismissInstrument() {}

    // IsReadyToPayServiceHelper.ResultHandler:
    @Override
    public void onIsReadyToPayServiceError() {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> respondToIsReadyToPayQuery(false));
    }

    @Override
    public void onIsReadyToPayServiceResponse(boolean isReadyToPay) {
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> respondToIsReadyToPayQuery(isReadyToPay));
    }
}

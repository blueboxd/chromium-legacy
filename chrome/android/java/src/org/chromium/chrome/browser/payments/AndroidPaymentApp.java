// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.IsReadyToPayService;
import org.chromium.IsReadyToPayServiceCallback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
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
public class AndroidPaymentApp extends PaymentApp implements WindowAndroid.IntentCallback {
    /** The maximum number of milliseconds to wait for a response from a READY_TO_PAY service. */
    private static final long READY_TO_PAY_TIMEOUT_MS = 400;

    /** The maximum number of milliseconds to wait for a connection to READY_TO_PAY service. */
    private static final long SERVICE_CONNECTION_TIMEOUT_MS = 1000;

    private static final String EMPTY_JSON_DATA = "{}";

    private final Handler mHandler;
    private final WebContents mWebContents;
    private final Set<String> mMethodNames;
    private final boolean mIsIncognito;
    private final String mPackageName;
    private final String mPayActivityName;
    private final String mIsReadyToPayServiceName;
    private IsReadyToPayCallback mIsReadyToPayCallback;
    private InstrumentDetailsCallback mInstrumentDetailsCallback;
    private ServiceConnection mServiceConnection;
    @Nullable
    private String mApplicationIdentifierToHide;
    private boolean mIsReadyToPayQueried;
    private boolean mIsServiceBindingInitiated;
    private boolean mBypassIsReadyToPayServiceInTest;

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param webContents         The web contents.
     * @param packageName         The name of the package of the payment app.
     * @param activity            The name of the payment activity in the payment app.
     * @param isReadyToPayService The name of the service that can answer "is ready to pay" query,
     *                            or null of none.
     * @param label               The UI label to use for the payment app.
     * @param icon                The icon to use in UI for the payment app.
     * @param isIncognito         Whether the user is in incognito mode.
     * @param appToHide           The identifier of the application that this app can hide.
     */
    public AndroidPaymentApp(WebContents webContents, String packageName, String activity,
            @Nullable String isReadyToPayService, String label, Drawable icon, boolean isIncognito,
            @Nullable String appToHide) {
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
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mIsReadyToPayCallback
                == null : "Have not responded to previous IS_READY_TO_PAY request";

        mIsReadyToPayCallback = callback;
        if (mIsReadyToPayServiceName == null) {
            respondToIsReadyToPayQuery(true);
            return;
        }

        assert !mIsIncognito;
        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                IsReadyToPayService isReadyToPayService =
                        IsReadyToPayService.Stub.asInterface(service);
                if (isReadyToPayService == null) {
                    respondToIsReadyToPayQuery(false);
                } else {
                    sendIsReadyToPayIntentToPaymentApp(isReadyToPayService);
                }
            }

            // "Called when a connection to the Service has been lost. This typically happens when
            // the process hosting the service has crashed or been killed. This does not remove the
            // ServiceConnection itself -- this binding to the service will remain active, and you
            // will receive a call to onServiceConnected(ComponentName, IBinder) when the Service is
            // next running."
            // https://developer.android.com/reference/android/content/ServiceConnection.html#onServiceDisconnected(android.content.ComponentName)
            @Override
            public void onServiceDisconnected(ComponentName name) {
                // Do not wait for the service to restart.
                respondToIsReadyToPayQuery(false);
            }
        };

        Intent isReadyToPayIntent = WebPaymentIntentHelper.createIsReadyToPayIntent(
                /*packageName=*/mPackageName, /*serviceName=*/mIsReadyToPayServiceName,
                removeUrlScheme(origin), removeUrlScheme(iframeOrigin), certificateChain,
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentMethodDataMap(methodDataMap));

        if (mBypassIsReadyToPayServiceInTest) {
            respondToIsReadyToPayQuery(true);
            return;
        }

        try {
            // This method returns "true if the system is in the process of bringing up a service
            // that your client has permission to bind to; false if the system couldn't find the
            // service or if your client doesn't have permission to bind to it. If this value is
            // true, you should later call unbindService(ServiceConnection) to release the
            // connection."
            // https://developer.android.com/reference/android/content/Context.html#bindService(android.content.Intent,%20android.content.ServiceConnection,%20int)
            mIsServiceBindingInitiated = ContextUtils.getApplicationContext().bindService(
                    isReadyToPayIntent, mServiceConnection, Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            // Intentionally blank, so mIsServiceBindingInitiated is false.
        }

        if (!mIsServiceBindingInitiated) {
            respondToIsReadyToPayQuery(false);
            return;
        }

        mHandler.postDelayed(() -> {
            if (!mIsReadyToPayQueried) respondToIsReadyToPayQuery(false);
        }, SERVICE_CONNECTION_TIMEOUT_MS);
    }

    @VisibleForTesting
    /* package */ void bypassIsReadyToPayServiceInTest() {
        mBypassIsReadyToPayServiceInTest = true;
    }

    private void respondToIsReadyToPayQuery(boolean isReadyToPay) {
        ThreadUtils.assertOnUiThread();
        if (mServiceConnection != null) {
            if (mIsServiceBindingInitiated) {
                // mServiceConnection "parameter must not be null."
                // https://developer.android.com/reference/android/content/Context.html#unbindService(android.content.ServiceConnection)
                ContextUtils.getApplicationContext().unbindService(mServiceConnection);
                mIsServiceBindingInitiated = false;
            }
            mServiceConnection = null;
        }

        if (mIsReadyToPayCallback == null) return;
        mIsReadyToPayCallback.onIsReadyToPayResponse(/*app=*/this, isReadyToPay);
        mIsReadyToPayCallback = null;
    }

    private void sendIsReadyToPayIntentToPaymentApp(IsReadyToPayService isReadyToPayService) {
        ThreadUtils.assertOnUiThread();
        if (mIsReadyToPayCallback == null) return;
        mIsReadyToPayQueried = true;
        IsReadyToPayServiceCallback.Stub callback = new IsReadyToPayServiceCallback.Stub() {
            @Override
            public void handleIsReadyToPay(boolean isReadyToPay) throws RemoteException {
                PostTask.runOrPostTask(
                        UiThreadTaskTraits.DEFAULT, () -> respondToIsReadyToPayQuery(isReadyToPay));
            }
        };
        try {
            isReadyToPayService.isReadyToPay(callback);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on to
            // the Service caller, see writeException in Parcel.java.
            respondToIsReadyToPayQuery(false);
            return;
        }
        mHandler.postDelayed(() -> respondToIsReadyToPayQuery(false), READY_TO_PAY_TIMEOUT_MS);
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
                    certificateChain, methodDataMap, total, displayItems, modifiers);
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
                                        total, displayItems, modifiers))
                .setNegativeButton(R.string.cancel,
                        (OnClickListener) (dialog, which)
                                -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .setOnCancelListener(
                        dialog -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .show();
    }

    private static String removeUrlScheme(String url) {
        return UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }

    private void launchPaymentApp(String id, String merchantName, String origin,
            String iframeOrigin, byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap, PaymentItem total,
            List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers) {
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mInstrumentDetailsCallback != null;

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
                WebPaymentIntentHelperTypeConverter.fromMojoPaymentDetailsModifierMap(modifiers));
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
        WebPaymentIntentHelper.parsePaymentResponse(resultCode, data,
                (errorString)
                        -> notifyErrorInvokingPaymentApp(errorString),
                (methodName, details)
                        -> mInstrumentDetailsCallback.onInstrumentDetailsReady(
                                methodName, details, new PayerData()));
        mInstrumentDetailsCallback = null;
    }

    @Override
    public void dismissInstrument() {}
}

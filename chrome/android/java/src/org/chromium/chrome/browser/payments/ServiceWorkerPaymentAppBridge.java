// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentEventResponseType;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;

import java.net.URI;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Native bridge for interacting with service worker based payment apps.
 */
@JNIAdditionalImport({PaymentInstrument.class})
public class ServiceWorkerPaymentAppBridge implements PaymentAppFactoryInterface {
    private static final String TAG = "SWPaymentApp";
    private static boolean sCanMakePaymentForTesting;
    private final Set<String> mStandardizedPaymentMethods = new HashSet<>();

    /** The interface for checking whether there is an installed SW payment app. */
    public static interface HasServiceWorkerPaymentAppsCallback {
        /**
         * Called to return checking result.
         *
         * @param hasPaymentApps Indicates whether there is an installed SW payment app.
         */
        public void onHasServiceWorkerPaymentAppsResponse(boolean hasPaymentApps);
    }

    /** The interface for getting all installed SW payment apps' information. */
    public static interface GetServiceWorkerPaymentAppsInfoCallback {
        /**
         * Called to return installed SW payment apps' information.
         *
         * @param appsInfo Contains all installed SW payment apps' information.
         */
        public void onGetServiceWorkerPaymentAppsInfo(Map<String, Pair<String, Bitmap>> appsInfo);
    }

    /* package */ ServiceWorkerPaymentAppBridge() {
        mStandardizedPaymentMethods.add(MethodStrings.BASIC_CARD);
        mStandardizedPaymentMethods.add(MethodStrings.INTERLEDGER);
        mStandardizedPaymentMethods.add(MethodStrings.PAYEE_CREDIT_TRANSFER);
        mStandardizedPaymentMethods.add(MethodStrings.PAYER_CREDIT_TRANSFER);
        mStandardizedPaymentMethods.add(MethodStrings.TOKENIZED_CARD);
    }

    // PaymentAppFactoryInterface implementation:
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        PaymentHandlerFinder finder = new PaymentHandlerFinder(delegate);
        ServiceWorkerPaymentAppBridgeJni.get().getAllPaymentApps(
                delegate.getParams().getWebContents(),
                delegate.getParams().getMethodData().values().toArray(
                        new PaymentMethodData[delegate.getParams().getMethodData().size()]),
                delegate.getParams().getMayCrawl(),
                /*callback=*/finder);
    }

    /** Finds the payment handlers and checks their "canmakepayment" response. */
    public class PaymentHandlerFinder {
        private final PaymentAppFactoryDelegate mDelegate;
        private int mNumberOfPendingCanMakePaymentEvents;
        private boolean mAreAllPaymentAppsCreated;

        /**
         * True if at least one payment handler with matching payment methods is found, regardless
         * of its reply to the "canmakepayment" event.
         */
        private boolean mPaymentHandlerWithMatchingMethodFound;

        private PaymentHandlerFinder(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        /** Called when an installed payment handler is found. */
        @CalledByNative("PaymentHandlerFinder")
        private void onInstalledPaymentHandlerFound(long registrationId, String scope,
                @Nullable String name, @Nullable String userHint, String origin,
                @Nullable Bitmap icon, String[] methodNameArray, boolean explicitlyVerified,
                Object[] capabilities, String[] preferredRelatedApplications,
                Object supportedDelegations) {
            ThreadUtils.assertOnUiThread();

            WebContents webContents = mDelegate.getParams().getWebContents();
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            if (activity == null) return;

            URI scopeUri = UriUtils.parseUriFromString(scope);
            if (scopeUri == null) {
                Log.e(TAG, "%s service worker scope is not a valid URI", scope);
                return;
            }

            ServiceWorkerPaymentApp app = new ServiceWorkerPaymentApp(webContents, registrationId,
                    scopeUri, name, userHint, origin,
                    icon == null ? null : new BitmapDrawable(activity.getResources(), icon),
                    methodNameArray, (ServiceWorkerPaymentApp.Capabilities[]) capabilities,
                    preferredRelatedApplications, (SupportedDelegations) supportedDelegations);
            mPaymentHandlerWithMatchingMethodFound = true;
            mNumberOfPendingCanMakePaymentEvents++;

            if (sCanMakePaymentForTesting || activity.getCurrentTabModel().isIncognito()
                    || mStandardizedPaymentMethods.containsAll(Arrays.asList(methodNameArray))
                    || !explicitlyVerified) {
                onCanMakePaymentEventResponse(app, true);
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

            fireCanMakePaymentEvent(webContents, registrationId, scope,
                    mDelegate.getParams().getId(), mDelegate.getParams().getTopLevelOrigin(),
                    mDelegate.getParams().getPaymentRequestOrigin(), supportedRequestedMethodData,
                    supportedRequestedModifiers, /*callback=*/this, app);
        }

        /** Called when an installable payment handler is found. */
        @CalledByNative("PaymentHandlerFinder")
        private void onInstallablePaymentHandlerFound(@Nullable String name, String swUrl,
                String scope, boolean useCache, @Nullable Bitmap icon, String methodName,
                String[] preferredRelatedApplications, Object supportedDelegations) {
            ThreadUtils.assertOnUiThread();

            WebContents webContents = mDelegate.getParams().getWebContents();
            Context context = ChromeActivity.fromWebContents(webContents);
            if (context == null) return;
            URI swUri = UriUtils.parseUriFromString(swUrl);
            if (swUri == null) {
                Log.e(TAG, "%s service worker installation url is not a valid URI", swUrl);
                return;
            }

            URI scopeUri = UriUtils.parseUriFromString(scope);
            if (scopeUri == null) {
                Log.e(TAG, "%s service worker scope is not a valid URI", scope);
                return;
            }

            mDelegate.onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents, name,
                    scopeUri.getHost(), swUri, scopeUri, useCache,
                    icon == null ? null : new BitmapDrawable(context.getResources(), icon),
                    methodName, preferredRelatedApplications,
                    (SupportedDelegations) supportedDelegations));
            mPaymentHandlerWithMatchingMethodFound = true;
            if (mNumberOfPendingCanMakePaymentEvents == 0 && mAreAllPaymentAppsCreated) {
                notifyFinished();
            }
        }

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         */
        @CalledByNative("PaymentHandlerFinder")
        private void onGetPaymentAppsError(String errorMessage) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        /** Called when the factory is finished creating payment apps. */
        @CalledByNative("PaymentHandlerFinder")
        private void onAllPaymentAppsCreated() {
            ThreadUtils.assertOnUiThread();
            mAreAllPaymentAppsCreated = true;
            if (mNumberOfPendingCanMakePaymentEvents == 0 && mAreAllPaymentAppsCreated) {
                notifyFinished();
            }
        }

        /**
         * Called when a service worker responds to the "canmakepayment" event.
         * @param app      The service worker that has responded to the "canmakepayment" event.
         * @param response The response from the service worker.
         */
        @CalledByNative("PaymentHandlerFinder")
        private void onCanMakePaymentEventResponse(
                ServiceWorkerPaymentApp app, boolean canMakePaymentEventResponse) {
            if (canMakePaymentEventResponse) mDelegate.onPaymentAppCreated(app);

            if (--mNumberOfPendingCanMakePaymentEvents == 0 && mAreAllPaymentAppsCreated) {
                notifyFinished();
            }
        }

        private void notifyFinished() {
            mDelegate.onCanMakePaymentCalculated(mPaymentHandlerWithMatchingMethodFound);
            mDelegate.onDoneCreatingPaymentApps(ServiceWorkerPaymentAppBridge.this);
        }
    }

    /**
     * Checks whether there is a installed SW payment app.
     *
     * @param callback The callback to return result.
     */
    public static void hasServiceWorkerPaymentApps(HasServiceWorkerPaymentAppsCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    callback.onHasServiceWorkerPaymentAppsResponse(false);
                }
            });
            return;
        }
        ServiceWorkerPaymentAppBridgeJni.get().hasServiceWorkerPaymentApps(callback);
    }

    /**
     * Gets all installed SW payment apps' information.
     *
     * @param callback The callback to return result.
     */
    public static void getServiceWorkerPaymentAppsInfo(
            GetServiceWorkerPaymentAppsInfoCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    callback.onGetServiceWorkerPaymentAppsInfo(
                            new HashMap<String, Pair<String, Bitmap>>());
                }
            });
            return;
        }
        ServiceWorkerPaymentAppBridgeJni.get().getServiceWorkerPaymentAppsInfo(callback);
    }

    /**
     * Returns whether the app can make a payment.
     *
     * @param webContents      The web contents that invoked PaymentRequest.
     * @param registrationId   The service worker registration ID of the Payment App.
     * @param swScope          The service worker scope.
     * @param paymentRequestId The payment request identifier.
     * @param origin           The origin of this merchant.
     * @param iframeOrigin     The origin of the iframe that invoked PaymentRequest. Same as origin
     *                         if PaymentRequest was not invoked from inside an iframe.
     * @param methodData       The PaymentMethodData objects that are relevant for this payment
     *                         app.
     * @param modifiers        Payment method specific modifiers to the payment items and the total.
     * @param callback         Called after the payment app is finished running.
     * @param app              The payment handler where the event is being fired.
     */
    private static void fireCanMakePaymentEvent(WebContents webContents, long registrationId,
            String swScope, String paymentRequestId, String origin, String iframeOrigin,
            Set<PaymentMethodData> methodData, Set<PaymentDetailsModifier> modifiers,
            PaymentHandlerFinder callback, ServiceWorkerPaymentApp app) {
        ThreadUtils.assertOnUiThread();

        ServiceWorkerPaymentAppBridgeJni.get().fireCanMakePaymentEvent(webContents, registrationId,
                swScope, paymentRequestId, origin, iframeOrigin,
                methodData.toArray(new PaymentMethodData[0]),
                modifiers.toArray(new PaymentDetailsModifier[0]), callback, app);
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

    /**
     * Invoke a payment app with a given option and matching method data.
     *
     * @param webContents      The web contents that invoked PaymentRequest.
     * @param registrationId   The service worker registration ID of the Payment App.
     * @param swScope          The scope of the service worker.
     * @param origin           The origin of this merchant.
     * @param iframeOrigin     The origin of the iframe that invoked PaymentRequest. Same as origin
     *                         if PaymentRequest was not invoked from inside an iframe.
     * @param paymentRequestId The unique identifier of the PaymentRequest.
     * @param methodData       The PaymentMethodData objects that are relevant for this payment
     *                         app.
     * @param total            The PaymentItem that represents the total cost of the payment.
     * @param modifiers        Payment method specific modifiers to the payment items and the total.
     * @param host             The host of the payment handler.
     * @param isMicrotrans     Whether the payment handler should be invoked in the microtransaction
     *                         mode.
     * @param callback         Called after the payment app is finished running.
     */
    public static void invokePaymentApp(WebContents webContents, long registrationId,
            String swScope, String origin, String iframeOrigin, String paymentRequestId,
            Set<PaymentMethodData> methodData, PaymentItem total,
            Set<PaymentDetailsModifier> modifiers, PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions, PaymentHandlerHost host,
            boolean isMicrotrans, PaymentInstrument.InstrumentDetailsCallback callback) {
        ThreadUtils.assertOnUiThread();

        ServiceWorkerPaymentAppBridgeJni.get().invokePaymentApp(webContents, registrationId,
                swScope, origin, iframeOrigin, paymentRequestId,
                methodData.toArray(new PaymentMethodData[0]), total,
                modifiers.toArray(new PaymentDetailsModifier[0]), paymentOptions,
                shippingOptions.toArray(new PaymentShippingOption[0]),
                host.getNativePaymentHandlerHost(), isMicrotrans, callback);
    }

    /**
     * Install and invoke a payment app with a given option and matching method data.
     *
     * @param webContents           The web contents that invoked PaymentRequest.
     * @param origin                The origin of this merchant.
     * @param iframeOrigin          The origin of the iframe that invoked PaymentRequest. Same as
     *                              origin if PaymentRequest was not invoked from inside an iframe.
     * @param paymentRequestId      The unique identifier of the PaymentRequest.
     * @param methodData            The PaymentMethodData objects that are relevant for this payment
     *                              app.
     * @param total                 The PaymentItem that represents the total cost of the payment.
     * @param modifiers             Payment method specific modifiers to the payment items and the
     *                              total.
     * @param host                  The host of the payment handler.
     * @param callback              Called after the payment app is finished running.
     * @param appName               The installable app name.
     * @param icon                  The installable app icon.
     * @param swUri                 The URI to get the app's service worker js script.
     * @param scope                 The scope of the service worker that should be registered.
     * @param useCache              Whether to use cache when registering the service worker.
     * @param method                Supported method name of the app.
     * @param supportedDelegations  Supported delegations of the app.
     */
    public static void installAndInvokePaymentApp(WebContents webContents, String origin,
            String iframeOrigin, String paymentRequestId, Set<PaymentMethodData> methodData,
            PaymentItem total, Set<PaymentDetailsModifier> modifiers, PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions, PaymentHandlerHost host,
            PaymentInstrument.InstrumentDetailsCallback callback, String appName,
            @Nullable Bitmap icon, URI swUri, URI scope, boolean useCache, String method,
            SupportedDelegations supportedDelegations) {
        ThreadUtils.assertOnUiThread();

        ServiceWorkerPaymentAppBridgeJni.get().installAndInvokePaymentApp(webContents, origin,
                iframeOrigin, paymentRequestId, methodData.toArray(new PaymentMethodData[0]), total,
                modifiers.toArray(new PaymentDetailsModifier[0]), paymentOptions,
                shippingOptions.toArray(new PaymentShippingOption[0]),
                host.getNativePaymentHandlerHost(), callback, appName, icon, swUri.toString(),
                scope.toString(), useCache, method, supportedDelegations.getShippingAddress(),
                supportedDelegations.getPayerName(), supportedDelegations.getPayerEmail(),
                supportedDelegations.getPayerPhone());
    }

    /**
     * Abort invocation of the payment app.
     *
     * @param webContents      The web contents that invoked PaymentRequest.
     * @param registrationId   The service worker registration ID of the Payment App.
     * @param swScope          The service worker scope.
     * @param paymentRequestId The payment request identifier.
     * @param callback         Called after abort invoke payment app is finished running.
     */
    public static void abortPaymentApp(WebContents webContents, long registrationId, String swScope,
            String paymentRequestId, PaymentInstrument.AbortCallback callback) {
        ThreadUtils.assertOnUiThread();

        ServiceWorkerPaymentAppBridgeJni.get().abortPaymentApp(
                webContents, registrationId, swScope, paymentRequestId, callback);
    }

    /**
     * Add observer for the opened payment app window tab so as to validate whether the web
     * contents is secure.
     *
     * @param tab The opened payment app window tab.
     */
    public static void addTabObserverForPaymentRequestTab(Tab tab) {
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Notify closing payment app window so as to abort payment if unsecure.
                WebContents webContents = tab.getWebContents();
                if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(webContents)) {
                    onClosingPaymentAppWindowForInsecureNavigation(webContents);
                }
            }

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                onClosingPaymentAppWindowForInsecureNavigation(tab.getWebContents());
            }
        });
    }

    /**
     * Notify closing the opened payment app window for insecure navigation.
     *
     * @param webContents The web contents in the opened window.
     */
    public static void onClosingPaymentAppWindowForInsecureNavigation(WebContents webContents) {
        ServiceWorkerPaymentAppBridgeJni.get().onClosingPaymentAppWindow(
                webContents, PaymentEventResponseType.PAYMENT_HANDLER_INSECURE_NAVIGATION);
    }

    /**
     * Notify closing the opened payment app window.
     *
     * @param webContents The web contents in the opened window.
     */
    public static void onClosingPaymentAppWindow(WebContents webContents) {
        ServiceWorkerPaymentAppBridgeJni.get().onClosingPaymentAppWindow(
                webContents, PaymentEventResponseType.PAYMENT_HANDLER_WINDOW_CLOSING);
    }

    @CalledByNative
    private static String getSupportedMethodFromMethodData(PaymentMethodData data) {
        return data.supportedMethod;
    }

    @CalledByNative
    private static String getStringifiedDataFromMethodData(PaymentMethodData data) {
        return data.stringifiedData;
    }

    @CalledByNative
    private static int[] getSupportedNetworksFromMethodData(PaymentMethodData data) {
        return data.supportedNetworks;
    }

    @CalledByNative
    private static PaymentMethodData getMethodDataFromModifier(PaymentDetailsModifier modifier) {
        return modifier.methodData;
    }

    @CalledByNative
    private static PaymentItem getTotalFromModifier(PaymentDetailsModifier modifier) {
        return modifier.total;
    }

    @CalledByNative
    private static String getLabelFromPaymentItem(PaymentItem item) {
        return item.label;
    }

    @CalledByNative
    private static String getCurrencyFromPaymentItem(PaymentItem item) {
        return item.amount.currency;
    }

    @CalledByNative
    private static String getValueFromPaymentItem(PaymentItem item) {
        return item.amount.value;
    }

    @CalledByNative
    private static boolean getRequestShippingFromPaymentOptions(PaymentOptions options) {
        return options.requestShipping;
    }

    @CalledByNative
    private static boolean getRequestPayerNameFromPaymentOptions(PaymentOptions options) {
        return options.requestPayerName;
    }

    @CalledByNative
    private static boolean getRequestPayerPhoneFromPaymentOptions(PaymentOptions options) {
        return options.requestPayerPhone;
    }

    @CalledByNative
    private static boolean getRequestPayerEmailFromPaymentOptions(PaymentOptions options) {
        return options.requestPayerEmail;
    }

    @CalledByNative
    private static String getIdFromPaymentShippingOption(PaymentShippingOption option) {
        return option.id;
    }

    @CalledByNative
    private static String getLabelFromPaymentShippingOption(PaymentShippingOption option) {
        return option.label;
    }

    @CalledByNative
    private static PaymentCurrencyAmount getAmountFromPaymentShippingOption(
            PaymentShippingOption option) {
        return option.amount;
    }

    @CalledByNative
    private static boolean getSelectedFromPaymentShippingOption(PaymentShippingOption option) {
        return option.selected;
    }

    @CalledByNative
    private static String getCurrencyFromPaymentCurrencyAmount(PaymentCurrencyAmount amount) {
        return amount.currency;
    }

    @CalledByNative
    private static String getValueFromPaymentCurrencyAmount(PaymentCurrencyAmount amount) {
        return amount.value;
    }

    @CalledByNative
    private static int getShippingTypeFromPaymentOptions(PaymentOptions options) {
        return options.shippingType;
    }

    @CalledByNative
    private static Object[] createCapabilities(int count) {
        return new ServiceWorkerPaymentApp.Capabilities[count];
    }

    @CalledByNative
    private static Object createSupportedDelegations(
            boolean shippingAddress, boolean payerName, boolean payerPhone, boolean payerEmail) {
        return new SupportedDelegations(shippingAddress, payerName, payerPhone, payerEmail);
    }

    @CalledByNative
    private static void addCapabilities(
            Object[] capabilities, int index, int[] supportedCardNetworks) {
        assert index < capabilities.length;
        capabilities[index] = new ServiceWorkerPaymentApp.Capabilities(supportedCardNetworks);
    }

    @CalledByNative
    private static void onHasServiceWorkerPaymentApps(
            HasServiceWorkerPaymentAppsCallback callback, boolean hasPaymentApps) {
        ThreadUtils.assertOnUiThread();

        callback.onHasServiceWorkerPaymentAppsResponse(hasPaymentApps);
    }

    @CalledByNative
    private static Object createPaymentAppsInfo() {
        return new HashMap<String, Pair<String, Bitmap>>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void addPaymentAppInfo(
            Object appsInfo, String scope, @Nullable String name, @Nullable Bitmap icon) {
        ((Map<String, Pair<String, Bitmap>>) appsInfo).put(scope, new Pair<>(name, icon));
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void onGetServiceWorkerPaymentAppsInfo(
            GetServiceWorkerPaymentAppsInfoCallback callback, Object appsInfo) {
        ThreadUtils.assertOnUiThread();

        callback.onGetServiceWorkerPaymentAppsInfo(((Map<String, Pair<String, Bitmap>>) appsInfo));
    }

    @CalledByNative
    private static Object createPayerData(String payerName, String payerPhone, String payerEmail,
            Object shippingAddress, String selectedShippingOptionId) {
        return new PayerData(payerName, payerPhone, payerEmail, (PaymentAddress) shippingAddress,
                selectedShippingOptionId);
    }

    @CalledByNative
    private static Object createShippingAddress(String country, String[] addressLine, String region,
            String city, String dependentLocality, String postalCode, String sortingCode,
            String organization, String recipient, String phone) {
        PaymentAddress result = new PaymentAddress();
        result.country = country;
        result.addressLine = addressLine;
        result.region = region;
        result.city = city;
        result.dependentLocality = dependentLocality;
        result.postalCode = postalCode;
        result.sortingCode = sortingCode;
        result.organization = organization;
        result.recipient = recipient;
        result.phone = phone;
        return result;
    }

    @CalledByNative
    private static void onPaymentAppInvoked(PaymentInstrument.InstrumentDetailsCallback callback,
            String methodName, String stringifiedDetails, Object payerData, String errorMessage) {
        ThreadUtils.assertOnUiThread();

        if (!TextUtils.isEmpty(errorMessage)) {
            assert TextUtils.isEmpty(methodName);
            assert TextUtils.isEmpty(stringifiedDetails);
            callback.onInstrumentDetailsError(errorMessage);
        } else {
            assert !TextUtils.isEmpty(methodName);
            assert !TextUtils.isEmpty(stringifiedDetails);
            callback.onInstrumentDetailsReady(
                    methodName, stringifiedDetails, (PayerData) payerData);
        }
    }

    @CalledByNative
    private static void onPaymentAppAborted(
            PaymentInstrument.AbortCallback callback, boolean result) {
        ThreadUtils.assertOnUiThread();

        callback.onInstrumentAbortResult(result);
    }

    @NativeMethods
    interface Natives {
        void getAllPaymentApps(WebContents webContents, PaymentMethodData[] methodData,
                boolean mayCrawlForInstallablePaymentApps, PaymentHandlerFinder callback);
        void hasServiceWorkerPaymentApps(HasServiceWorkerPaymentAppsCallback callback);
        void getServiceWorkerPaymentAppsInfo(GetServiceWorkerPaymentAppsInfoCallback callback);
        void invokePaymentApp(WebContents webContents, long registrationId,
                String serviceWorkerScope, String topOrigin, String paymentRequestOrigin,
                String paymentRequestId, PaymentMethodData[] methodData, PaymentItem total,
                PaymentDetailsModifier[] modifiers, PaymentOptions paymentOptions,
                PaymentShippingOption[] shippingOptions, long nativePaymentHandlerObject,
                boolean isMicrotrans, PaymentInstrument.InstrumentDetailsCallback callback);
        void installAndInvokePaymentApp(WebContents webContents, String topOrigin,
                String paymentRequestOrigin, String paymentRequestId,
                PaymentMethodData[] methodData, PaymentItem total,
                PaymentDetailsModifier[] modifiers, PaymentOptions paymentOptions,
                PaymentShippingOption[] shippingOptions, long nativePaymentHandlerObject,
                PaymentInstrument.InstrumentDetailsCallback callback, String appName,
                @Nullable Bitmap icon, String swUrl, String scope, boolean useCache, String method,
                boolean supportedDelegationsShippingAddress, boolean supportedDelegationsPayerName,
                boolean supportedDelegationsPayerEmail, boolean supportedDelegationsPayerPhone);
        void abortPaymentApp(WebContents webContents, long registrationId,
                String serviceWorkerScope, String paymentRequestId,
                PaymentInstrument.AbortCallback callback);
        void fireCanMakePaymentEvent(WebContents webContents, long registrationId,
                String serviceWorkerScope, String paymentRequestId, String topOrigin,
                String paymentRequestOrigin, PaymentMethodData[] methodData,
                PaymentDetailsModifier[] modifiers, PaymentHandlerFinder callback,
                ServiceWorkerPaymentApp app);
        void onClosingPaymentAppWindow(WebContents webContents, int reason);
    }
}

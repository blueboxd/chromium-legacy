// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;
import org.chromium.payments.mojom.PaymentShippingOption;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This app class represents a service worker based payment app.
 *
 * Such apps are implemented as service workers according to the Payment
 * Handler API specification.
 *
 * @see https://w3c.github.io/payment-handler/
 */
public class ServiceWorkerPaymentApp extends PaymentApp {
    private final WebContents mWebContents;
    private final long mRegistrationId;
    private final GURL mScope;
    private final Set<String> mMethodNames;
    private final Capabilities[] mCapabilities;
    private final boolean mCanPreselect;
    private final Set<String> mPreferredRelatedApplicationIds;
    private final SupportedDelegations mSupportedDelegations;

    // Below variables are used for installable service worker payment app specifically.
    private final boolean mNeedsInstallation;
    private final String mAppName;
    private final GURL mSwUrl;
    private final boolean mUseCache;

    /* The endpoint for payment handler communication, such as the
     * change-[payment-method|shipping-address|shipping-option] events.
     */
    private PaymentHandlerHost mPaymentHandlerHost;

    /** Whether the app can show its own UI when it is invoked. */
    private boolean mCanShowOwnUI = true;

    /** Whether the app is ready for minial UI flow. */
    private boolean mIsReadyForMinimalUI;

    /** UKM source Id generated using the app's origin. */
    private long mUkmSourceId;

    /** The account balance to be used in the minimal UI flow. */
    @Nullable
    private String mAccountBalance;

    /**
     * This class represents capabilities of a payment instrument. It is currently only used for
     * 'basic-card' payment instrument.
     */
    protected static class Capabilities {
        // Stores mojom::BasicCardNetwork.
        private int[] mSupportedCardNetworks;

        /**
         * Build capabilities for a payment instrument.
         *
         * @param supportedCardNetworks The supported card networks of a 'basic-card' payment
         *                              instrument.
         */
        /* package */ Capabilities(int[] supportedCardNetworks) {
            mSupportedCardNetworks = supportedCardNetworks;
        }

        /**
         * Gets supported card networks.
         *
         * @return a set of mojom::BasicCardNetwork.
         */
        /* package */ int[] getSupportedCardNetworks() {
            return mSupportedCardNetworks;
        }
    }

    /**
     * Build a service worker payment app instance per origin.
     *
     * @see https://w3c.github.io/webpayments-payment-handler/#structure-of-a-web-payment-app
     *
     * @param webContents                    The web contents where PaymentRequest was invoked.
     * @param registrationId                 The registration id of the corresponding service worker
     *                                       payment app.
     * @param scope                          The registration scope of the corresponding service
     *                                       worker.
     * @param name                           The name of the payment app.
     * @param userHint                       The user hint of the payment app.
     * @param icon                           The drawable icon of the payment app.
     * @param methodNames                    A set of payment method names supported by the payment
     *                                       app.
     * @param capabilities                   A set of capabilities of the payment instruments in
     *                                       this payment app (only valid for basic-card payment
     *                                       method for now).
     * @param preferredRelatedApplicationIds A set of preferred related application Ids.
     * @param supportedDelegations           Supported delegations of the payment app.
     */
    public ServiceWorkerPaymentApp(WebContents webContents, long registrationId, GURL scope,
            @Nullable String name, @Nullable String userHint, @Nullable BitmapDrawable icon,
            String[] methodNames, Capabilities[] capabilities,
            String[] preferredRelatedApplicationIds, SupportedDelegations supportedDelegations) {
        // Do not display duplicate information.
        super(scope.getSpec(), TextUtils.isEmpty(name) ? scope.getHost() : name, userHint,
                TextUtils.isEmpty(name) ? null : scope.getHost(), icon);
        mWebContents = webContents;
        mRegistrationId = registrationId;
        mScope = scope;

        // Name and/or icon are set to null if fetching or processing the corresponding web
        // app manifest failed. Then do not preselect this payment app.
        mCanPreselect = !TextUtils.isEmpty(name) && icon != null;

        mMethodNames = new HashSet<>();
        for (int i = 0; i < methodNames.length; i++) {
            mMethodNames.add(methodNames[i]);
        }

        mCapabilities = Arrays.copyOf(capabilities, capabilities.length);

        mPreferredRelatedApplicationIds = new HashSet<>();
        Collections.addAll(mPreferredRelatedApplicationIds, preferredRelatedApplicationIds);

        mSupportedDelegations = supportedDelegations;

        mNeedsInstallation = false;
        mAppName = name;
        mSwUrl = null;
        mUseCache = false;
        mUkmSourceId = 0;
    }

    /**
     * Build a service worker payment app instance which has not been installed yet.
     * The payment app will be installed when paying with it.
     *
     * @param webContents                     The web contents where PaymentRequest was invoked.
     * @param name                            The name of the payment app.
     * @param swUrl                           The URL to get the service worker js script.
     * @param scope                           The registration scope of the corresponding service
     *                                        worker.
     * @param useCache                        Whether cache is used to register the service worker.
     * @param icon                            The drawable icon of the payment app.
     * @param methodName                      The supported method name.
     * @param preferredRelatedApplicationIds  A set of preferred related application Ids.
     * @param supportedDelegations            Supported delegations of the payment app.
     */
    public ServiceWorkerPaymentApp(WebContents webContents, @Nullable String name, GURL swUrl,
            GURL scope, boolean useCache, @Nullable BitmapDrawable icon, String methodName,
            String[] preferredRelatedApplicationIds, SupportedDelegations supportedDelegations) {
        // Do not display duplicate information.
        super(scope.getSpec(), TextUtils.isEmpty(name) ? scope.getHost() : name, null,
                TextUtils.isEmpty(name) ? null : scope.getHost(), icon);

        mWebContents = webContents;
        // No registration ID before the app is registered (installed).
        mRegistrationId = -1;
        mScope = scope;
        // If name and/or icon is missing or failed to parse from the web app manifest, then do not
        // preselect this payment app.
        mCanPreselect = !TextUtils.isEmpty(name) && icon != null;
        mMethodNames = new HashSet<>();
        mMethodNames.add(methodName);
        // Installable payment apps must be default application of a payment method.
        mCapabilities = new Capabilities[0];
        mPreferredRelatedApplicationIds = new HashSet<>();
        Collections.addAll(mPreferredRelatedApplicationIds, preferredRelatedApplicationIds);

        mSupportedDelegations = supportedDelegations;

        mNeedsInstallation = true;
        mAppName = name;
        mSwUrl = swUrl;
        mUseCache = useCache;
        mUkmSourceId = 0;
    }

    /**
     * Sets the endpoint for payment handler communication. Must be called before invoking this
     * payment handler.
     * @param host The endpoint for payment handler communication. Should not be null.
     */
    /* package */ void setPaymentHandlerHost(PaymentHandlerHost host) {
        assert host != null;
        mPaymentHandlerHost = host;
    }

    /*package*/ GURL getScope() {
        return mScope;
    }

    // Matches ||requestMethodData|.supportedNetwokrs for 'basic-card' payment method with the
    // Capabilities in this payment app to determine whether this payment app supports
    // |requestMethodData|.
    private boolean matchBasiccardCapabilities(PaymentMethodData requestMethodData) {
        assert requestMethodData != null;
        // Empty supported card networks in payment request method data indicates it supports all
        // card types and networks.
        if (requestMethodData.supportedNetworks.length == 0) return true;

        // Payment app with emtpy capabilities can only match payment request method data with empty
        // supported card types and networks.
        if (mCapabilities.length == 0) return false;

        Set<Integer> requestSupportedNetworks = new HashSet<>();
        for (int i = 0; i < requestMethodData.supportedNetworks.length; i++) {
            requestSupportedNetworks.add(requestMethodData.supportedNetworks[i]);
        }

        // If requestSupportedNetworks are not empty, match them with the capabilities. Break out of
        // the for loop if a matched capability has been found. So 'j < mCapabilities.length'
        // indicates that there is a matched capability in this payment app.
        int j = 0;
        for (; j < mCapabilities.length; j++) {
            if (!requestSupportedNetworks.isEmpty()) {
                int[] supportedNetworks = mCapabilities[j].getSupportedCardNetworks();

                Set<Integer> capabilitiesSupportedCardNetworks = new HashSet<>();
                for (int i = 0; i < supportedNetworks.length; i++) {
                    capabilitiesSupportedCardNetworks.add(supportedNetworks[i]);
                }

                capabilitiesSupportedCardNetworks.retainAll(requestSupportedNetworks);
                if (capabilitiesSupportedCardNetworks.isEmpty()) continue;
            }

            break;
        }
        return j < mCapabilities.length;
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public boolean isValidForPaymentMethodData(String method, @Nullable PaymentMethodData data) {
        boolean isSupportedMethod = super.isValidForPaymentMethodData(method, data);
        if (isSupportedMethod && MethodStrings.BASIC_CARD.equals(method) && data != null) {
            return matchBasiccardCapabilities(data);
        }
        return isSupportedMethod;
    }

    @Override
    public void invokePaymentApp(String id, String merchantName, String origin, String iframeOrigin,
            byte[][] unusedCertificateChain, Map<String, PaymentMethodData> methodData,
            PaymentItem total, List<PaymentItem> displayItems,
            Map<String, PaymentDetailsModifier> modifiers, PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions, InstrumentDetailsCallback callback) {
        assert mPaymentHandlerHost != null;
        if (mNeedsInstallation) {
            assert mCanShowOwnUI;
            BitmapDrawable icon = (BitmapDrawable) getDrawableIcon();
            ServiceWorkerPaymentAppBridge.installAndInvokePaymentApp(mWebContents, origin,
                    iframeOrigin, id, new HashSet<>(methodData.values()), total,
                    new HashSet<>(modifiers.values()), paymentOptions, shippingOptions,
                    mPaymentHandlerHost, callback, mAppName, icon == null ? null : icon.getBitmap(),
                    mSwUrl, mScope, mUseCache, mMethodNames.toArray(new String[0])[0],
                    mSupportedDelegations);
        } else {
            ServiceWorkerPaymentAppBridge.invokePaymentApp(mWebContents, mRegistrationId, mScope,
                    origin, iframeOrigin, id, new HashSet<>(methodData.values()), total,
                    new HashSet<>(modifiers.values()), paymentOptions, shippingOptions,
                    mPaymentHandlerHost, mCanShowOwnUI, callback);
        }
    }

    @Override
    public void updateWith(PaymentRequestDetailsUpdate response) {
        assert isWaitingForPaymentDetailsUpdate();
        mPaymentHandlerHost.updateWith(response);
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        assert isWaitingForPaymentDetailsUpdate();
        mPaymentHandlerHost.onPaymentDetailsNotUpdated();
    }

    @Override
    public boolean isWaitingForPaymentDetailsUpdate() {
        return mPaymentHandlerHost != null
                && mPaymentHandlerHost.isWaitingForPaymentDetailsUpdate();
    }

    @Override
    public void abortPaymentApp(String id, AbortCallback callback) {
        ServiceWorkerPaymentAppBridge.abortPaymentApp(
                mWebContents, mRegistrationId, mScope, id, callback);
    }

    @Override
    public void dismissInstrument() {}

    @Override
    public void setIsReadyForMinimalUI(boolean isReadyForMinimalUI) {
        mIsReadyForMinimalUI = isReadyForMinimalUI;
    }

    @Override
    public boolean isReadyForMinimalUI() {
        return mIsReadyForMinimalUI;
    }

    @Override
    public void setAccountBalance(@Nullable String accountBalance) {
        mAccountBalance = accountBalance;
    }

    @Override
    @Nullable
    public String accountBalance() {
        return mAccountBalance;
    }

    @Override
    public void disableShowingOwnUI() {
        mCanShowOwnUI = false;
    }

    @Override
    public boolean canPreselect() {
        return mCanPreselect;
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

    @Override
    @Nullable
    public Set<String> getApplicationIdentifiersThatHideThisApp() {
        return mPreferredRelatedApplicationIds;
    }

    @Override
    public long getUkmSourceId() {
        if (mUkmSourceId == 0) {
            mUkmSourceId = ServiceWorkerPaymentAppBridge.getSourceIdForPaymentAppFromScope(mScope);
        }
        return mUkmSourceId;
    }
}

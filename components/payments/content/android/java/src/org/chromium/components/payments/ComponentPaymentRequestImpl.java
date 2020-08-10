// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.autofill.EditableOption;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentValidationErrors;

import java.util.List;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This component provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.PaymentRequestImpl.
 */
public class ComponentPaymentRequestImpl implements PaymentRequest {
    private static PaymentRequestServiceObserverForTest sObserverForTest;
    private static NativeObserverForTest sNativeObserverForTest;
    private final BrowserPaymentRequestFactory mBrowserPaymentRequestFactory;
    private final RenderFrameHost mRenderFrameHost;
    private boolean mSkipUiForNonUrlPaymentMethodIdentifiers;
    private BrowserPaymentRequest mBrowserPaymentRequest;
    private PaymentRequestClient mClient;
    private boolean mIsOffTheRecord;
    private PaymentRequestLifecycleObserver mPaymentRequestLifecycleObserver;

    /** The factory that creates an instance of {@link BrowserPaymentRequest}. */
    public interface BrowserPaymentRequestFactory {
        /**
         * Create an instance of {@link BrowserPaymentRequest}, and have it working together with an
         * instance of {@link ComponentPaymentRequestImpl}.
         * @param componentPaymentRequestImpl The ComponentPaymentRequestImpl to work together with
         *         the BrowserPaymentRequest instance.
         * @param isOffTheRecord Whether the merchant page is in a OffTheRecord (e.g., incognito,
         *         guest mode) Tab.
         * @param journeyLogger The logger that records the user journey of PaymentRequest.
         */
        BrowserPaymentRequest createBrowserPaymentRequest(
                ComponentPaymentRequestImpl componentPaymentRequestImpl, boolean isOffTheRecord,
                JourneyLogger journeyLogger);
    }

    /**
     * An observer interface injected when running tests to allow them to observe events.
     * This interface holds events that should be passed back to the native C++ test
     * harness and mirrors the C++ PaymentRequest::ObserverForTest() interface. Its methods
     * should be called in the same places that the C++ PaymentRequest object will call its
     * ObserverForTest.
     */
    public interface NativeObserverForTest {
        void onCanMakePaymentCalled();
        void onCanMakePaymentReturned();
        void onHasEnrolledInstrumentCalled();
        void onHasEnrolledInstrumentReturned();
        void onAppListReady(@Nullable List<EditableOption> paymentApps, PaymentItem total);
        void onNotSupportedError();
        void onConnectionTerminated();
        void onAbortCalled();
        void onCompleteCalled();
        void onMinimalUIReady();
    }

    /**
     * A test-only observer for the PaymentRequest service implementation.
     */
    public interface PaymentRequestServiceObserverForTest {
        /**
         * Called after an instance of {@link ComponentPaymentRequestImpl} has been created.
         *
         * @param componentPaymentRequest The newly created instance of ComponentPaymentRequestImpl.
         */
        void onPaymentRequestCreated(ComponentPaymentRequestImpl componentPaymentRequest);

        /**
         * Called when an abort request was denied.
         */
        void onPaymentRequestServiceUnableToAbort();

        /**
         * Called when the controller is notified of billing address change, but does not alter the
         * editor UI.
         */
        void onPaymentRequestServiceBillingAddressChangeProcessed();

        /**
         * Called when the controller is notified of an expiration month change.
         */
        void onPaymentRequestServiceExpirationMonthChange();

        /**
         * Called when a show request failed. This can happen when:
         * <ul>
         *   <li>The merchant requests only unsupported payment methods.</li>
         *   <li>The merchant requests only payment methods that don't have corresponding apps and
         *   are not able to add a credit card from PaymentRequest UI.</li>
         * </ul>
         */
        void onPaymentRequestServiceShowFailed();

        /**
         * Called when the canMakePayment() request has been responded to.
         */
        void onPaymentRequestServiceCanMakePaymentQueryResponded();

        /**
         * Called when the hasEnrolledInstrument() request has been responded to.
         */
        void onPaymentRequestServiceHasEnrolledInstrumentQueryResponded();

        /**
         * Called when the payment response is ready.
         */
        void onPaymentResponseReady();

        /**
         * Called when the browser acknowledges the renderer's complete call, which indicates that
         * the browser UI has closed.
         */
        void onCompleteReplied();

        /**
         * Called when the renderer is closing the mojo connection (e.g. upon show promise
         * rejection).
         */
        void onRendererClosedMojoConnection();
    }

    /**
     * Build an instance of the PaymentRequest implementation.
     * @param renderFrameHost The RenderFrameHost of the merchant page.
     * @param isOffTheRecord Whether the merchant page is in a OffTheRecord (e.g., incognito, guest
     *         mode) Tab.
     * @param skipUiForBasicCard True if the PaymentRequest UI should be skipped when the request
     *         only supports basic-card methods.
     * @param browserPaymentRequestFactory The factory that generates an instance of
     *         BrowserPaymentRequest to work with this ComponentPaymentRequestImpl instance.
     */
    public static ComponentPaymentRequestImpl create(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, boolean skipUiForBasicCard,
            BrowserPaymentRequestFactory browserPaymentRequestFactory) {
        ComponentPaymentRequestImpl instance = new ComponentPaymentRequestImpl(
                renderFrameHost, isOffTheRecord, skipUiForBasicCard, browserPaymentRequestFactory);
        instance.onCreated();
        return instance;
    }

    private ComponentPaymentRequestImpl(RenderFrameHost renderFrameHost, boolean isOffTheRecord,
            boolean skipUiForBasicCard, BrowserPaymentRequestFactory browserPaymentRequestFactory) {
        mBrowserPaymentRequestFactory = browserPaymentRequestFactory;
        mIsOffTheRecord = isOffTheRecord;
        mSkipUiForNonUrlPaymentMethodIdentifiers = skipUiForBasicCard;
        mRenderFrameHost = renderFrameHost;
    }

    private void onCreated() {
        if (sObserverForTest == null) return;
        sObserverForTest.onPaymentRequestCreated(this);
    }

    /**
     * Set a native-side observer for PaymentRequest implementations. This observer should be set
     * before PaymentRequest implementations are instantiated.
     * @param nativeObserverForTest The native-side observer.
     */
    @VisibleForTesting
    public static void setNativeObserverForTest(NativeObserverForTest nativeObserverForTest) {
        sNativeObserverForTest = nativeObserverForTest;
    }

    /** @return Get the native=side observer, for testing purpose only. */
    @Nullable
    public static NativeObserverForTest getNativeObserverForTest() {
        return sNativeObserverForTest;
    }

    @Override
    public void init(PaymentRequestClient client, PaymentMethodData[] methodData,
            PaymentDetails details, PaymentOptions options, boolean googlePayBridgeEligible) {
        JourneyLogger journeyLogger = new JourneyLogger(
                mIsOffTheRecord, WebContentsStatics.fromRenderFrameHost(mRenderFrameHost));
        mBrowserPaymentRequest = mBrowserPaymentRequestFactory.createBrowserPaymentRequest(
                this, mIsOffTheRecord, journeyLogger);
        assert mBrowserPaymentRequest != null;
        if (mClient != null) {
            mBrowserPaymentRequest.getJourneyLogger().setAborted(
                    AbortReason.INVALID_DATA_FROM_RENDERER);
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(
                    ErrorStrings.ATTEMPTED_INITIALIZATION_TWICE);
            return;
        }

        if (client == null) {
            journeyLogger.setAborted(AbortReason.INVALID_DATA_FROM_RENDERER);
            mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(ErrorStrings.INVALID_STATE);
            return;
        }

        mClient = client;

        mBrowserPaymentRequest.init(methodData, details, options, googlePayBridgeEligible);
    }

    @Override
    public void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        mBrowserPaymentRequest.show(isUserGesture, waitForUpdatedDetails);
    }

    @Override
    public void updateWith(PaymentDetails details) {
        mBrowserPaymentRequest.updateWith(details);
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        mBrowserPaymentRequest.onPaymentDetailsNotUpdated();
    }

    @Override
    public void abort() {
        mBrowserPaymentRequest.abort();
    }

    @Override
    public void complete(int result) {
        mBrowserPaymentRequest.complete(result);
    }

    @Override
    public void retry(PaymentValidationErrors errors) {
        mBrowserPaymentRequest.retry(errors);
    }

    @Override
    public void canMakePayment() {
        mBrowserPaymentRequest.canMakePayment();
    }

    @Override
    public void hasEnrolledInstrument(boolean perMethodQuota) {
        mBrowserPaymentRequest.hasEnrolledInstrument(perMethodQuota);
    }

    @Override
    public void close() {
        mBrowserPaymentRequest.close();
    }

    @Override
    public void onConnectionError(MojoException e) {
        mBrowserPaymentRequest.onConnectionError(e);
    }

    /**
     * Get the PaymentRequestClient. To close the client, the caller should call {@link
     * #closeClient()} instead of calling {@link PaymentRequest#close()} directly.
     * @return The client.
     */
    @Nullable
    public PaymentRequestClient getClient() {
        return mClient;
    }

    /**
     * Close the PaymentRequestClient.
     */
    public void closeClient() {
        if (mClient != null) mClient.close();
        mClient = null;
    }

    /**
     * Register an observer for the PaymentRequest lifecycle.
     * @param paymentRequestLifecycleObserver The observer.
     */
    public void registerPaymentRequestLifecycleObserver(
            PaymentRequestLifecycleObserver paymentRequestLifecycleObserver) {
        mPaymentRequestLifecycleObserver = paymentRequestLifecycleObserver;
    }

    /** @return The observer for the PaymentRequest lifecycle. */
    public PaymentRequestLifecycleObserver getPaymentRequestLifecycleObserver() {
        return mPaymentRequestLifecycleObserver;
    }

    /** @return An observer for the payment request service, if any; otherwise, null. */
    @Nullable
    public static PaymentRequestServiceObserverForTest getObserverForTest() {
        return sObserverForTest;
    }

    /** Set an observer for the payment request service, cannot be null. */
    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        assert observerForTest != null;
        sObserverForTest = observerForTest;
    }

    /**
     * @return True when skip UI is available for non-url based payment method identifiers (e.g.
     * basic-card).
     */
    public boolean skipUiForNonUrlPaymentMethodIdentifiers() {
        return mSkipUiForNonUrlPaymentMethodIdentifiers;
    }

    @VisibleForTesting
    public void setSkipUiForNonUrlPaymentMethodIdentifiersForTest() {
        mSkipUiForNonUrlPaymentMethodIdentifiers = true;
    }
}

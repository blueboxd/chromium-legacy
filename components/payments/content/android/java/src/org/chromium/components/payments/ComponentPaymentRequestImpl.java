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
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentRequestClient;
import org.chromium.payments.mojom.PaymentResponse;
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
    private PaymentRequestClient mClient;
    private boolean mIsOffTheRecord;
    private PaymentRequestLifecycleObserver mPaymentRequestLifecycleObserver;
    private boolean mHasTorndown;
    @Nullable
    private BrowserPaymentRequest mBrowserPaymentRequest;

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
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.show(isUserGesture, waitForUpdatedDetails);
    }

    @Override
    public void updateWith(PaymentDetails details) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.updateWith(details);
    }

    @Override
    public void onPaymentDetailsNotUpdated() {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.onPaymentDetailsNotUpdated();
    }

    @Override
    public void abort() {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.abort();
    }

    @Override
    public void complete(int result) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.complete(result);
    }

    @Override
    public void retry(PaymentValidationErrors errors) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.retry(errors);
    }

    @Override
    public void canMakePayment() {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.canMakePayment();
    }

    @Override
    public void hasEnrolledInstrument(boolean perMethodQuota) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.hasEnrolledInstrument(perMethodQuota);
    }

    // This should be called by the renderer only. The closing triggered by other classes should
    // call {@link #teardown} instead.
    @Override
    public void close() {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.getJourneyLogger().setAborted(
                org.chromium.components.payments.AbortReason.MOJO_RENDERER_CLOSING);
        teardown();
        if (sObserverForTest != null) {
            sObserverForTest.onRendererClosedMojoConnection();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    @Override
    public void onConnectionError(MojoException e) {
        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.getJourneyLogger().setAborted(
                org.chromium.components.payments.AbortReason.MOJO_CONNECTION_ERROR);
        teardown();
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Close this instance and release all of the retained resources. The external callers of this
     * method should stop referencing this instance upon calling. This method can be called within
     * itself without causing dead loops.
     */
    public void teardown() {
        if (mHasTorndown) return;
        mHasTorndown = true;

        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.close();
        mBrowserPaymentRequest = null;

        if (mClient != null) mClient.close();
        mClient = null;
    }

    /**
     * Register an observer for the PaymentRequest lifecycle.
     * @param paymentRequestLifecycleObserver The observer, cannot be null.
     */
    public void registerPaymentRequestLifecycleObserver(
            PaymentRequestLifecycleObserver paymentRequestLifecycleObserver) {
        assert paymentRequestLifecycleObserver != null;
        mPaymentRequestLifecycleObserver = paymentRequestLifecycleObserver;
    }

    /** @return The observer for the PaymentRequest lifecycle, can be null. */
    @Nullable
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

    /** Invokes {@link PaymentRequest.onPaymentMethodChange}. */
    public void onPaymentMethodChange(String methodName, String stringifiedDetails) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
    }

    /** Invokes {@link PaymentRequest.onShippingAddressChange}. */
    public void onShippingAddressChange(PaymentAddress address) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onShippingAddressChange(address);
    }

    /** Invokes {@link PaymentRequest.onShippingOptionChange}. */
    public void onShippingOptionChange(String shippingOptionId) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onShippingOptionChange(shippingOptionId);
    }

    /** Invokes {@link PaymentRequest.onPayerDetailChange}. */
    public void onPayerDetailChange(PayerDetail detail) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onPayerDetailChange(detail);
    }

    /** Invokes {@link PaymentRequest.onPaymentResponse}. */
    public void onPaymentResponse(PaymentResponse response) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onPaymentResponse(response);
    }

    /** Invokes {@link PaymentRequest.onError}. */
    public void onError(int error, String errorMessage) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onError(error, errorMessage);
    }

    /** Invokes {@link PaymentRequest.onComplete}. */
    public void onComplete() {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onComplete();
    }

    /** Invokes {@link PaymentRequest.onAbort}. */
    public void onAbort(boolean abortedSuccessfully) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onAbort(abortedSuccessfully);
    }

    /** Invokes {@link PaymentRequest.onCanMakePayment}. */
    public void onCanMakePayment(int result) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onCanMakePayment(result);
    }

    /** Invokes {@link PaymentRequest.onHasEnrolledInstrument}. */
    public void onHasEnrolledInstrument(int result) {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.onHasEnrolledInstrument(result);
    }

    /** Invokes {@link PaymentRequest.warnNoFavicon}. */
    public void warnNoFavicon() {
        // Every caller should stop referencing this class once teardown() is called.
        assert mClient != null;

        mClient.warnNoFavicon();
    }
}

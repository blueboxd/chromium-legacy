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
 * {@link ComponentPaymentRequestImpl}, {@link MojoPaymentRequestGateKeeper} and PaymentRequestImpl
 * together make up the PaymentRequest service defined in
 * third_party/blink/public/mojom/payments/payment_request.mojom. This class provides the parts
 * shareable between Clank and WebLayer. The Clank specific logic lives in
 * org.chromium.chrome.browser.payments.PaymentRequestImpl.
 * TODO(crbug.com/1102522): PaymentRequestImpl is under refactoring, with the purpose of moving the
 * business logic of PaymentRequestImpl into ComponentPaymentRequestImpl and eventually moving
 * PaymentRequestImpl. Note that the callers of the instances of this class need to close them with
 * {@link ComponentPaymentRequestImpl#close()}, after which no usage is allowed.
 */
public class ComponentPaymentRequestImpl {
    private static PaymentRequestServiceObserverForTest sObserverForTest;
    private static NativeObserverForTest sNativeObserverForTest;
    private final Runnable mOnClosedListener;
    private boolean mSkipUiForNonUrlPaymentMethodIdentifiers;
    private PaymentRequestLifecycleObserver mPaymentRequestLifecycleObserver;
    private boolean mHasClosed;

    // After create(), mClient is null only when it has closed.
    @Nullable
    private PaymentRequestClient mClient;

    // After the constructor, mBrowserPaymentRequest is null only when it has closed.
    @Nullable
    private BrowserPaymentRequest mBrowserPaymentRequest;

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
     * Create an instance of {@link PaymentRequest} that provides the Android PaymentRequest
     * service.
     * @param renderFrameHost The RenderFrameHost of the merchant page.
     * @param isOffTheRecord Whether the merchant page is in a off-the-record (e.g., incognito,
     *         guest mode) Tab.
     * @param skipUiForBasicCard True if the PaymentRequest UI should be skipped when the request
     *         only supports basic-card methods.
     * @param browserPaymentRequestFactory The factory that generates BrowserPaymentRequest.
     * @return The created instance.
     */
    public static PaymentRequest createPaymentRequest(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, boolean skipUiForBasicCard,
            BrowserPaymentRequest.Factory browserPaymentRequestFactory) {
        return new MojoPaymentRequestGateKeeper(
                (client, methodData, details, options, googlePayBridgeEligible, onClosedListener)
                        -> ComponentPaymentRequestImpl.createIfParamsValid(renderFrameHost,
                                isOffTheRecord, skipUiForBasicCard, browserPaymentRequestFactory,
                                client, methodData, details, options, googlePayBridgeEligible,
                                onClosedListener));
    }

    /**
     * @return An instance of {@link ComponentPaymentRequestImpl} only if the parameters are deemed
     *         valid; Otherwise, null.
     */
    @Nullable
    private static ComponentPaymentRequestImpl createIfParamsValid(RenderFrameHost renderFrameHost,
            boolean isOffTheRecord, boolean skipUiForBasicCard,
            BrowserPaymentRequest.Factory browserPaymentRequestFactory,
            @Nullable PaymentRequestClient client, @Nullable PaymentMethodData[] methodData,
            @Nullable PaymentDetails details, @Nullable PaymentOptions options,
            boolean googlePayBridgeEligible, Runnable onClosedListener) {
        assert renderFrameHost != null;
        assert browserPaymentRequestFactory != null;
        assert onClosedListener != null;

        ComponentPaymentRequestImpl instance = new ComponentPaymentRequestImpl(renderFrameHost,
                isOffTheRecord, skipUiForBasicCard, browserPaymentRequestFactory, onClosedListener);
        instance.onCreated();
        boolean valid = instance.initAndValidate(
                client, methodData, details, options, googlePayBridgeEligible);
        if (!valid) {
            instance.close();
            return null;
        }
        return instance;
    }

    private void onCreated() {
        if (sObserverForTest == null) return;
        sObserverForTest.onPaymentRequestCreated(this);
    }

    private ComponentPaymentRequestImpl(RenderFrameHost renderFrameHost, boolean isOffTheRecord,
            boolean skipUiForBasicCard, BrowserPaymentRequest.Factory browserPaymentRequestFactory,
            Runnable onClosedListener) {
        mSkipUiForNonUrlPaymentMethodIdentifiers = skipUiForBasicCard;
        JourneyLogger journeyLogger = new JourneyLogger(
                isOffTheRecord, WebContentsStatics.fromRenderFrameHost(renderFrameHost));
        mBrowserPaymentRequest = browserPaymentRequestFactory.createBrowserPaymentRequest(
                renderFrameHost, this, isOffTheRecord, journeyLogger);
        assert mBrowserPaymentRequest != null;
        mOnClosedListener = onClosedListener;
        mHasClosed = false;
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

    private boolean initAndValidate(@Nullable PaymentRequestClient client,
            @Nullable PaymentMethodData[] methodData, @Nullable PaymentDetails details,
            @Nullable PaymentOptions options, boolean googlePayBridgeEligible) {
        if (client == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_STATE);
            return false;
        }
        mClient = client;
        if (methodData == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_PAYMENT_METHODS_OR_DATA);
            return false;
        }
        if (details == null) {
            abortForInvalidDataFromRenderer(ErrorStrings.INVALID_PAYMENT_DETAILS);
            return false;
        }

        assert mBrowserPaymentRequest != null;
        return mBrowserPaymentRequest.initAndValidate(
                methodData, details, options, googlePayBridgeEligible);
    }

    /**
     * The component part of the {@link PaymentRequest#show} implementation. Check {@link
     * PaymentRequest#show} for the parameters' specification.
     */
    /* package */ void show(boolean isUserGesture, boolean waitForUpdatedDetails) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.show(isUserGesture, waitForUpdatedDetails);
    }

    /**
     * The component part of the {@link PaymentRequest#updateWith} implementation.
     * @param details The details that the merchant provides to update the payment request.
     */
    /* package */ void updateWith(PaymentDetails details) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.updateWith(details);
    }

    /**
     * The component part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     */
    /* package */ void onPaymentDetailsNotUpdated() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.onPaymentDetailsNotUpdated();
    }

    /** The component part of the {@link PaymentRequest#abort} implementation. */
    /* package */ void abort() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;
        mBrowserPaymentRequest.abort();
    }

    /** The component part of the {@link PaymentRequest#complete} implementation. */
    /* package */ void complete(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.complete(result);
    }

    /**
     * The component part of the {@link PaymentRequest#retry} implementation. Check {@link
     * PaymentRequest#retry} for the parameters' specification.
     */
    /* package */ void retry(PaymentValidationErrors errors) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.retry(errors);
    }

    /** The component part of the {@link PaymentRequest#canMakePayment} implementation. */
    /* package */ void canMakePayment() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.canMakePayment();
    }

    /**
     * The component part of the {@link PaymentRequest#hasEnrolledInstrument} implementation.
     * @param perMethodQuota Whether to query with per-method quota.
     */
    /* package */ void hasEnrolledInstrument(boolean perMethodQuota) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.hasEnrolledInstrument(perMethodQuota);
    }

    /**
     * Implement {@link PaymentRequest#close}. This should be called by the renderer only. The
     * closing triggered by other classes should call {@link #close} instead. The caller should
     * stop referencing this class after calling this method.
     */
    /* package */ void closeByRenderer() {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.getJourneyLogger().setAborted(AbortReason.MOJO_RENDERER_CLOSING);
        close();
        if (sObserverForTest != null) {
            sObserverForTest.onRendererClosedMojoConnection();
        }
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Called when the mojo connection with the renderer PaymentRequest has an error.  The caller
     * should stop referencing this class after calling this method.
     * @param e The mojo exception.
     */
    /* package */ void onConnectionError(MojoException e) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.getJourneyLogger().setAborted(AbortReason.MOJO_CONNECTION_ERROR);
        close();
        if (sNativeObserverForTest != null) {
            sNativeObserverForTest.onConnectionTerminated();
        }
    }

    /**
     * Abort the request because the (untrusted) renderer passes invalid data.
     * @param debugMessage The debug message to be sent to the renderer.
     */
    /* package */ void abortForInvalidDataFromRenderer(String debugMessage) {
        // Every caller should stop referencing this class once close() is called.
        assert mBrowserPaymentRequest != null;

        mBrowserPaymentRequest.getJourneyLogger().setAborted(
                AbortReason.INVALID_DATA_FROM_RENDERER);
        mBrowserPaymentRequest.disconnectFromClientWithDebugMessage(debugMessage);
    }

    /**
     * Close this instance and release all of the retained resources. The external callers of this
     * method should stop referencing this instance upon calling. This method can be called within
     * itself without causing infinite loops.
     */
    public void close() {
        if (mHasClosed) return;
        mHasClosed = true;

        if (mBrowserPaymentRequest == null) return;
        mBrowserPaymentRequest.close();
        mBrowserPaymentRequest = null;

        // mClient can be null only when this method is called from
        // ComponentPaymentRequestImpl#create().
        if (mClient != null) mClient.close();
        mClient = null;

        mOnClosedListener.run();
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
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPaymentMethodChange(methodName, stringifiedDetails);
    }

    /** Invokes {@link PaymentRequest.onShippingAddressChange}. */
    public void onShippingAddressChange(PaymentAddress address) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onShippingAddressChange(address);
    }

    /** Invokes {@link PaymentRequest.onShippingOptionChange}. */
    public void onShippingOptionChange(String shippingOptionId) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onShippingOptionChange(shippingOptionId);
    }

    /** Invokes {@link PaymentRequest.onPayerDetailChange}. */
    public void onPayerDetailChange(PayerDetail detail) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPayerDetailChange(detail);
    }

    /** Invokes {@link PaymentRequest.onPaymentResponse}. */
    public void onPaymentResponse(PaymentResponse response) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onPaymentResponse(response);
    }

    /** Invokes {@link PaymentRequest.onError}. */
    public void onError(int error, String errorMessage) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onError(error, errorMessage);
    }

    /** Invokes {@link PaymentRequest.onComplete}. */
    public void onComplete() {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onComplete();
    }

    /** Invokes {@link PaymentRequest.onAbort}. */
    public void onAbort(boolean abortedSuccessfully) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onAbort(abortedSuccessfully);
    }

    /** Invokes {@link PaymentRequest.onCanMakePayment}. */
    public void onCanMakePayment(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onCanMakePayment(result);
    }

    /** Invokes {@link PaymentRequest.onHasEnrolledInstrument}. */
    public void onHasEnrolledInstrument(int result) {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.onHasEnrolledInstrument(result);
    }

    /** Invokes {@link PaymentRequest.warnNoFavicon}. */
    public void warnNoFavicon() {
        // Every caller should stop referencing this class once close() is called.
        assert mClient != null;

        mClient.warnNoFavicon();
    }
}

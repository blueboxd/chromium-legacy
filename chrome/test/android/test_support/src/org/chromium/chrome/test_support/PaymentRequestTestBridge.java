// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.payments.PaymentRequestFactory;
import org.chromium.chrome.browser.payments.PaymentRequestImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Test support for injecting test behaviour from C++ tests into Java PaymentRequests.
 */
@JNINamespace("payments")
public class PaymentRequestTestBridge {
    /**
     * A test override of the PaymentRequestImpl's Delegate. Allows tests to control the answers
     * about the state of the system, in order to control which paths should be tested in the
     * PaymentRequestImpl.
     */
    private static class PaymentRequestDelegateForTest implements PaymentRequestImpl.Delegate {
        private final boolean mIsIncognito;
        private final boolean mIsValidSsl;
        private final boolean mIsWebContentsActive;
        private final boolean mPrefsCanMakePayment;
        private final boolean mSkipUiForBasicCard;

        PaymentRequestDelegateForTest(boolean isIncognito, boolean isValidSsl,
                boolean isWebContentsActive, boolean prefsCanMakePayment,
                boolean skipUiForBasicCard) {
            mIsIncognito = isIncognito;
            mIsValidSsl = isValidSsl;
            mIsWebContentsActive = isWebContentsActive;
            mPrefsCanMakePayment = prefsCanMakePayment;
            mSkipUiForBasicCard = skipUiForBasicCard;
        }

        @Override
        public boolean isIncognito(@Nullable ChromeActivity activity) {
            return mIsIncognito;
        }

        @Override
        public String getInvalidSslCertificateErrorMessage(WebContents webContents) {
            if (mIsValidSsl) return null;
            return "Invalid SSL certificate";
        }

        @Override
        public boolean isWebContentsActive(TabModel model, WebContents webContents) {
            return mIsWebContentsActive;
        }

        @Override
        public boolean prefsCanMakePayment() {
            return mPrefsCanMakePayment;
        }

        @Override
        public boolean skipUiForBasicCard() {
            return false;
        }
    }

    /**
     * Implements NativeObserverForTest by holding pointers to C++ callbacks, and invoking
     * them through nativeResolvePaymentRequestObserverCallback() when the observer's
     * methods are called.
     */
    private static class PaymentRequestNativeObserverBridgeToNativeForTest
            implements PaymentRequestImpl.NativeObserverForTest {
        private final long mOnCanMakePaymentCalledPtr;
        private final long mOnCanMakePaymentReturnedPtr;
        private final long mOnHasEnrolledInstrumentCalledPtr;
        private final long mOnHasEnrolledInstrumentReturnedPtr;
        private final long mOnShowAppsReadyPtr;
        private final long mOnNotSupportedErrorPtr;
        private final long mOnConnectionTerminatedPtr;
        private final long mOnAbortCalledPtr;
        private final long mOnCompleteCalledPtr;
        private final long mOnMinimalUIReadyPtr;

        PaymentRequestNativeObserverBridgeToNativeForTest(long onCanMakePaymentCalledPtr,
                long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
                long onHasEnrolledInstrumentReturnedPtr, long onShowAppsReadyPtr,
                long onNotSupportedErrorPtr, long onConnectionTerminatedPtr, long onAbortCalledPtr,
                long onCompleteCalledPtr, long onMinimalUIReadyPtr) {
            mOnCanMakePaymentCalledPtr = onCanMakePaymentCalledPtr;
            mOnCanMakePaymentReturnedPtr = onCanMakePaymentReturnedPtr;
            mOnHasEnrolledInstrumentCalledPtr = onHasEnrolledInstrumentCalledPtr;
            mOnHasEnrolledInstrumentReturnedPtr = onHasEnrolledInstrumentReturnedPtr;
            mOnShowAppsReadyPtr = onShowAppsReadyPtr;
            mOnNotSupportedErrorPtr = onNotSupportedErrorPtr;
            mOnConnectionTerminatedPtr = onConnectionTerminatedPtr;
            mOnAbortCalledPtr = onAbortCalledPtr;
            mOnCompleteCalledPtr = onCompleteCalledPtr;
            mOnMinimalUIReadyPtr = onMinimalUIReadyPtr;
        }

        @Override
        public void onCanMakePaymentCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnCanMakePaymentCalledPtr);
        }
        @Override
        public void onCanMakePaymentReturned() {
            nativeResolvePaymentRequestObserverCallback(mOnCanMakePaymentReturnedPtr);
        }
        @Override
        public void onHasEnrolledInstrumentCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnHasEnrolledInstrumentCalledPtr);
        }
        @Override
        public void onHasEnrolledInstrumentReturned() {
            nativeResolvePaymentRequestObserverCallback(mOnHasEnrolledInstrumentReturnedPtr);
        }
        @Override
        public void onShowAppsReady() {
            nativeResolvePaymentRequestObserverCallback(mOnShowAppsReadyPtr);
        }
        @Override
        public void onNotSupportedError() {
            nativeResolvePaymentRequestObserverCallback(mOnNotSupportedErrorPtr);
        }
        @Override
        public void onConnectionTerminated() {
            nativeResolvePaymentRequestObserverCallback(mOnConnectionTerminatedPtr);
        }
        @Override
        public void onAbortCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnAbortCalledPtr);
        }
        @Override
        public void onCompleteCalled() {
            nativeResolvePaymentRequestObserverCallback(mOnCompleteCalledPtr);
        }
        @Override
        public void onMinimalUIReady() {
            nativeResolvePaymentRequestObserverCallback(mOnMinimalUIReadyPtr);
        }
    }

    private static final String TAG = "PaymentRequestTestBridge";

    @CalledByNative
    public static void setUseDelegateForTest(boolean useDelegate, boolean isIncognito,
            boolean isValidSsl, boolean isWebContentsActive, boolean prefsCanMakePayment,
            boolean skipUiForBasicCard) {
        if (useDelegate) {
            PaymentRequestFactory.sDelegateForTest = new PaymentRequestDelegateForTest(isIncognito,
                    isValidSsl, isWebContentsActive, prefsCanMakePayment, skipUiForBasicCard);
        } else {
            PaymentRequestFactory.sDelegateForTest = null;
        }
    }

    @CalledByNative
    public static void setUseNativeObserverForTest(long onCanMakePaymentCalledPtr,
            long onCanMakePaymentReturnedPtr, long onHasEnrolledInstrumentCalledPtr,
            long onHasEnrolledInstrumentReturnedPtr, long onShowAppsReadyPtr,
            long onNotSupportedErrorPtr, long onConnectionTerminatedPtr, long onAbortCalledPtr,
            long onCompleteCalledPtr, long onMinimalUIReadyPtr) {
        PaymentRequestFactory.sNativeObserverForTest =
                new PaymentRequestNativeObserverBridgeToNativeForTest(onCanMakePaymentCalledPtr,
                        onCanMakePaymentReturnedPtr, onHasEnrolledInstrumentCalledPtr,
                        onHasEnrolledInstrumentReturnedPtr, onShowAppsReadyPtr,
                        onNotSupportedErrorPtr, onConnectionTerminatedPtr, onAbortCalledPtr,
                        onCompleteCalledPtr, onMinimalUIReadyPtr);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static WebContents getPaymentHandlerWebContentsForTest() {
        return PaymentRequestImpl.getPaymentHandlerWebContentsForTest();
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean clickPaymentHandlerSecurityIconForTest() {
        return PaymentRequestImpl.clickPaymentHandlerSecurityIconForTest();
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean confirmMinimalUIForTest() {
        return PaymentRequestImpl.confirmMinimalUIForTest();
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean dismissMinimalUIForTest() {
        return PaymentRequestImpl.dismissMinimalUIForTest();
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public static boolean isAndroidMarshmallowOrLollipopForTest() {
        return Build.VERSION.SDK_INT == Build.VERSION_CODES.M
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
                || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1;
    }

    /**
     * The native method responsible to executing RepeatingCallback pointers.
     */
    private static native void nativeResolvePaymentRequestObserverCallback(long callbackPtr);
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;

import java.nio.ByteBuffer;

/**
 * Native bridge for finding payment apps.
 */
public class PaymentAppServiceBridge implements PaymentAppFactoryInterface {
    /* package */ PaymentAppServiceBridge() {}

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        assert (delegate.getParams().getRenderFrameHost().getLastCommittedURL().equals(
                delegate.getParams().getPaymentRequestOrigin()));
        PaymentAppServiceCallback callback = new PaymentAppServiceCallback(delegate);
        PaymentAppServiceBridgeJni.get().create(delegate.getParams().getRenderFrameHost(),
                delegate.getParams().getTopLevelOrigin(),
                delegate.getParams()
                        .getMethodData()
                        .values()
                        .stream()
                        .map(data -> data.serialize())
                        .toArray(ByteBuffer[] ::new),
                delegate.getParams().getMayCrawl(), callback);
    }

    /** Handles callbacks from native PaymentAppService and creates PaymentApps. */
    public class PaymentAppServiceCallback {
        private final PaymentAppFactoryDelegate mDelegate;
        private boolean mCanMakePayment;

        private PaymentAppServiceCallback(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
            mCanMakePayment = false;
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreated() {
            ThreadUtils.assertOnUiThread();
            mCanMakePayment = true;
            // TODO(crbug.com/1063118): call mDelegate.onPaymentAppCreated().
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreationError(String errorMessage) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        // Expect to be called exactly once
        @CalledByNative("PaymentAppServiceCallback")
        private void onDoneCreatingPaymentApps() {
            ThreadUtils.assertOnUiThread();
            mDelegate.onCanMakePaymentCalculated(mCanMakePayment);
            mDelegate.onDoneCreatingPaymentApps(PaymentAppServiceBridge.this);
        }
    }

    @NativeMethods
    /* package */ interface Natives {
        void create(RenderFrameHost initiatorRenderFrameHost, String topOrigin,
                ByteBuffer[] methodData, boolean mayCrawlForInstallablePaymentApps,
                PaymentAppServiceCallback callback);
    }
}

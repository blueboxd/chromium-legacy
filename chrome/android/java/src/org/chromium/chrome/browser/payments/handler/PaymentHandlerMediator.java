// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.SslValidityChecker;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator
        extends WebContentsObserver implements BottomSheetObserver {
    private final PropertyModel mModel;
    private final Runnable mHider;

    /**
     * Build a new mediator that handle events from outside the payment handler component.
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *         payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *         hidden.
     * @param webContents The web-contents that loads the payment app.
     */
    /* package */ PaymentHandlerMediator(
            PropertyModel model, Runnable hider, WebContents webContents) {
        super(webContents);
        mModel = model;
        mHider = hider;
    }

    /**
     * Hide the bottom-sheet if weak-ref of web-contents refers to null.
     * @return Return true if the sheet is hidden.
     */
    private boolean hideSheetIfWebContentsNotExist() {
        if (mWebContents != null) return false;
        // TODO(maxlg): how to inform the service worker when the web-contents is missing.
        mHider.run();
        return true;
    }

    // BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        if (hideSheetIfWebContentsNotExist()) return;
        switch (newState) {
            case SheetState.HIDDEN:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContents.get());
                mHider.run();
                break;
        }
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        mModel.set(PaymentHandlerProperties.BOTTOM_SHEET_HEIGHT_FRACTION, heightFraction);
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {}

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    @Override
    public void onLoadUrl(String url) {}

    @Override
    public void onSheetFullyPeeked() {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // WebContentsObserver:
    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        if (hideSheetIfWebContentsNotExist()) return;
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mWebContents.get())) {
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                    mWebContents.get());
            mHider.run();
        }
    }

    @Override
    public void didAttachInterstitialPage() {
        if (hideSheetIfWebContentsNotExist()) return;
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                mWebContents.get());
        mHider.run();
    }

    @Override
    public void didFailLoad(
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
        if (hideSheetIfWebContentsNotExist()) return;
        // TODO(crbug.com/1017926): Respond to service worker with the net error.
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContents.get());
        mHider.run();
    }
}

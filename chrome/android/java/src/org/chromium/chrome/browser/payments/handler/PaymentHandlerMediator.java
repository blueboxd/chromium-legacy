// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.os.Handler;
import android.view.View;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.SslValidityChecker;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator extends WebContentsObserver
        implements BottomSheetObserver, PaymentHandlerToolbarObserver, View.OnLayoutChangeListener {
    // The value is picked in order to allow users to see the tab behind this UI.
    /* package */ static final float FULL_HEIGHT_RATIO = 0.9f;
    /* package */ static final float HALF_HEIGHT_RATIO = 0.5f;

    private final PropertyModel mModel;
    // Whenever invoked, invoked outside of the WebContentsObserver callbacks.
    private final Runnable mHider;
    // Postfixed with "Ref" to distinguish from mWebContent in WebContentsObserver. Although
    // referencing the same object, mWebContentsRef is preferable to WebContents here because
    // mWebContents (a weak ref) requires null checks, while mWebContentsRef is guaranteed to be not
    // null.
    private final WebContents mWebContentsRef;
    private final PaymentHandlerUiObserver mPaymentHandlerUiObserver;
    // Used to postpone execution of a callback to avoid destroy objects (e.g., WebContents) in
    // their own methods.
    private final Handler mHandler = new Handler();
    private final View mTabView;
    private final int mToolbarViewHeightPx;
    private final int mContainerTopPaddingPx;

    /**
     * Build a new mediator that handle events from outside the payment handler component.
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *         payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *         hidden.
     * @param webContents The web-contents that loads the payment app.
     * @param observer The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @param tabView The view of the main tab.
     * @param toolbarView The height of the PaymentHandler toolbar view.
     * @param containerTopPaddingPx The padding top of bottom_sheet_toolbar_container.
     */
    /* package */ PaymentHandlerMediator(PropertyModel model, Runnable hider,
            WebContents webContents, PaymentHandlerUiObserver observer, View tabView,
            int toolbarViewHeightPx, int containerTopPaddingPx) {
        super(webContents);
        assert webContents != null;
        mTabView = tabView;
        mWebContentsRef = webContents;
        mToolbarViewHeightPx = toolbarViewHeightPx;
        mModel = model;
        mHider = hider;
        mPaymentHandlerUiObserver = observer;
        mContainerTopPaddingPx = containerTopPaddingPx;
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());
    }

    @Override
    public void destroy() {
        super.destroy(); // Stops observing the web contents and cleans up associated references.
        mHandler.removeCallbacksAndMessages(null);
    }

    // View.OnLayoutChangeListener:
    // This is the Tab View's layout change listener, invoked in response to phone rotation.
    // TODO(crbug.com/1057825): It should listen to the BottomSheet container's layout change
    // instead of the Tab View layout change for better encapsulation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());
    }

    // BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
                mHandler.post(mHider);
                break;
        }
    }

    private static int getYLocationOnScreen(View view) {
        int[] point = new int[2];
        view.getLocationOnScreen(point);
        return point[1];
    }

    /** @return The height of visible area of the bottom sheet's content part. */
    private int contentVisibleHeight() {
        return (int) (mTabView.getHeight() * FULL_HEIGHT_RATIO) - mToolbarViewHeightPx
                - mContainerTopPaddingPx;
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mPaymentHandlerUiObserver.onPaymentHandlerUiShown();
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    @Override
    public void onSheetFullyPeeked() {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // WebContentsObserver:
    @Override
    public void didFinishNavigation(NavigationHandle navigationHandle) {
        if (navigationHandle.isSameDocument()) return;
        closeIfInsecure();
    }

    @Override
    public void didChangeVisibleSecurityState() {
        closeIfInsecure();
    }

    private void closeIfInsecure() {
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mWebContentsRef)) {
            closeUIForInsecureNavigation();
        }
    }

    @Override
    public void didAttachInterstitialPage() {
        closeUIForInsecureNavigation();
    }

    private void closeUIForInsecureNavigation() {
        mHandler.post(() -> {
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                    mWebContentsRef);
            mHider.run();
        });
    }

    @Override
    public void didFailLoad(boolean isMainFrame, int errorCode, String failingUrl) {
        mHandler.post(() -> {
            // TODO(crbug.com/1017926): Respond to service worker with the net error.
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
            mHider.run();
        });
    }

    // PaymentHandlerToolbarObserver:
    @Override
    public void onToolbarCloseButtonClicked() {
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
        mHandler.post(mHider);
    }
}

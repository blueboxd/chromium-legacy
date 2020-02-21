// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.os.Handler;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewConstraints;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * The view of the PaymentHandler UI. This view can be divided into the toolbar area and the
 * content area. The content area does not include the toolbar area; it includes the BottomSheet
 * area below the toolbar, which includes the part that extends beneath the screen. The ThinWebView
 * is designed to fit into the visible part of the content area, called content visible area.
 */
/* package */ class PaymentHandlerView implements BottomSheetContent {
    private final View mToolbarView;
    private final FrameLayout mContentView;
    private final ThinWebView mThinWebView;
    private final WebContents mWebContents;
    private final Handler mReflowHandler = new Handler();
    private final int mTabHeight;
    private final int mToolbarHeightPx;
    private final ChromeActivity mActivity;

    /**
     * Construct the PaymentHandlerView.
     *
     * @param activity The activity where the bottome-sheet should be shown.
     * @param webContents The web-content of the payment-handler web-app.
     * @param webContentView The {@link ContentView} that has been constructed with the web-content.
     * @param toolbarView The view of the Payment Handler toolbar.
     */
    /* package */ PaymentHandlerView(ChromeActivity activity, WebContents webContents,
            ContentView webContentView, View toolbarView) {
        mActivity = activity;
        mWebContents = webContents;
        mTabHeight = mActivity.getActivityTab().getView().getHeight();
        mToolbarView = toolbarView;
        mToolbarHeightPx =
                activity.getResources().getDimensionPixelSize(R.dimen.sheet_tab_toolbar_height);
        mContentView = (FrameLayout) LayoutInflater.from(activity).inflate(
                R.layout.payment_handler_content, null);

        mThinWebView = ThinWebViewFactory.create(
                activity, new ActivityWindowAndroid(activity), new ThinWebViewConstraints());
        initContentView(activity, mThinWebView, webContents, webContentView);
    }

    /**
     * Initialize the content view.
     *
     * @param activity The activity where the bottome-sheet should be shown.
     * @param thinWebView The {@link ThinWebView} that was created with the activity.
     * @param webContents The web-content of the payment-handler web-app.
     * @param webContentView The {@link ContentView} that has been constructed with the web-content.
     */
    private void initContentView(ChromeActivity activity, ThinWebView thinWebView,
            WebContents webContents, ContentView webContentView) {
        assert webContentView.getParent() == null;
        thinWebView.attachWebContents(webContents, webContentView);
        mContentView.setPadding(
                /*left=*/0, /*top=*/mToolbarHeightPx, /*right=*/0, /*bottom=*/0);
        mContentView.addView(thinWebView.getView(), /*index=*/0);
    }

    /**
     * Invoked when the visible area of the content container changes.
     * @param heightPx The height of the visible area of the Payment Handler UI content container,
     *         in pixels.
     */
    /* package */ void onContentVisibleHeightChanged(int heightPx) {
        // Reflow the web-content when the bottom-sheet size stops changing.
        mReflowHandler.removeCallbacksAndMessages(null);
        mReflowHandler.postDelayed(() -> reflowWebContents(heightPx), /*delayMillis=*/100);
    }

    /* Resize ThinWebView to reflow the web-contents. */
    private void reflowWebContents(int heightPx) {
        // Scale mThinWebView to make the web-content fit into the visible content area of the
        // PaymentHandler UI.
        if (mThinWebView.getView() == null || mWebContents.isDestroyed()) return;
        LayoutParams params = (LayoutParams) mThinWebView.getView().getLayoutParams();
        params.height = Math.max(0, heightPx);
        mThinWebView.getView().setLayoutParams(params);
    }

    /* package */ int getToolbarHeightPx() {
        return mToolbarHeightPx;
    }

    // BottomSheetContent:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public float getFullHeightRatio() {
        return 0.9f;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return true;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    @Override
    public void destroy() {
        mReflowHandler.removeCallbacksAndMessages(null);
        mThinWebView.destroy();
    }

    @Override
    @ContentPriority
    public int getPriority() {
        // If multiple bottom sheets are queued up to be shown, prioritize payment-handler, because
        // it's triggered by a user gesture, such as a click on <button>Buy this article</button>.
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.payment_handler_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.payment_handler_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.payment_handler_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.payment_handler_sheet_closed;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // flinging down hard enough will close the sheet.
        return true;
    }
}

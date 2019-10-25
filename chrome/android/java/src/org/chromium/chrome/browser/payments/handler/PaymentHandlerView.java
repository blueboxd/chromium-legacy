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

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.ui.widget.FadingShadow;
import org.chromium.chrome.browser.ui.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

/** PaymentHandler UI. */
/* package */ class PaymentHandlerView implements BottomSheetContent {
    private final int mToolbarHeightPx;
    private final View mToolbarView;
    private final FrameLayout mContentView;
    private final ThinWebView mThinWebView;
    private final Handler mReflowHandler = new Handler();
    private final int mTabHeight;

    /**
     * Construct the PaymentHandlerView.
     *
     * @param activity The activity where the bottome-sheet should be shown.
     * @param bottomSheet The bottom-sheet where the web-content should be shown.
     * @param webContents The web-content of the payment-handler web-app.
     * @param webContentView The {@link ContentView} that has been contructed with the web-content.
     */
    /* package */ PaymentHandlerView(ChromeActivity activity, BottomSheet bottomSheet,
            WebContents webContents, ContentView webContentView) {
        mToolbarHeightPx = activity.getResources().getDimensionPixelSize(
                R.dimen.custom_tabs_control_container_height);
        mTabHeight = activity.getActivityTab().getHeight();

        mToolbarView = LayoutInflater.from(activity).inflate(R.layout.custom_tabs_toolbar, null);
        mContentView = (FrameLayout) LayoutInflater.from(activity).inflate(
                R.layout.payment_handler_content, null);

        mThinWebView = ThinWebViewFactory.create(activity, new ActivityWindowAndroid(activity));
        initContentView(activity, mThinWebView, webContents, webContentView);
    }

    /**
     * Initialize the content view.
     *
     * @param activity The activity where the bottome-sheet should be shown.
     * @param thinWebView The {@link ThinWebView} that was created with the activity.
     * @param webContents The web-content of the payment-handler web-app.
     * @param webContentView The {@link ContentView} that has been contructed with the web-content.
     */
    private void initContentView(ChromeActivity activity, ThinWebView thinWebView,
            WebContents webContents, ContentView webContentView) {
        FadingShadowView shadow = mContentView.findViewById(R.id.shadow);
        shadow.init(ApiCompatibilityUtils.getColor(
                            activity.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);
        assert webContentView.getParent() == null;
        thinWebView.attachWebContents(webContents, webContentView);
        mContentView.setPadding(/*left=*/0, /*top=*/mToolbarHeightPx, /*right=*/0, /*bottom=*/0);
        mContentView.addView(thinWebView.getView(), /*index=*/0);
    }

    /* A callback when the heightFraction property changed.*/
    /* package */ void onHeightFractionChanged(float heightFraction) {
        // Reflow the web-content when the bottom-sheet size stops changing.
        mReflowHandler.removeCallbacksAndMessages(null);
        mReflowHandler.postDelayed(() -> reflowWebContents(heightFraction), /*delayMillis=*/100);
    }

    /* Resize ThinWebView to reflow the web-contents. */
    private void reflowWebContents(float heightFraction) {
        // Scale mThinWebView to make the web-content fit into the visible area of the bottom-sheet.
        if (mThinWebView.getView() == null) return;
        LayoutParams params = (LayoutParams) mThinWebView.getView().getLayoutParams();
        params.height = Math.max(0, (int) (mTabHeight * heightFraction) - mToolbarHeightPx);
        mThinWebView.getView().setLayoutParams(params);
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
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mReflowHandler.removeCallbacksAndMessages(null);
        mThinWebView.destroy();
    }

    @Override
    @BottomSheet.ContentPriority
    public int getPriority() {
        // If multiple bottom sheets are queued up to be shown, prioritize payment-handler, because
        // it's triggered by a user gesture, such as a click on <button>Buy this article</button>.
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheet.HeightMode.DISABLED;
    }

    @Override
    public boolean wrapContentEnabled() {
        return false;
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

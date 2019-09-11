// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Represents ephemeral tab content and the toolbar, which can be included inside the bottom sheet.
 */
public class EphemeralTabSheetContent implements BottomSheet.BottomSheetContent {
    private final Context mContext;
    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;

    private WebContents mWebContents;
    private ContentView mWebContentView;
    private ThinWebView mThinWebView;

    /**
     * Constructor.
     * @param context An Android context.
     */
    public EphemeralTabSheetContent(Context context) {
        mContext = context;

        createThinWebView();
        createToolbarView();
    }

    /**
     * Add web contents to the sheet.
     * @param webContents The {@link WebContents} to be displayed.
     * @param contentView The {@link ContentView} associated with the web contents.
     */
    public void attachWebContents(WebContents webContents, ContentView contentView) {
        mWebContents = webContents;
        mWebContentView = contentView;
        if (mWebContentView.getParent() != null) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }
        mThinWebView.attachWebContents(mWebContents, mWebContentView);
    }

    // Create a ThinWebView, add it to the view hierarchy, which represents the contents of the
    // bottom sheet.
    private void createThinWebView() {
        mThinWebView = ThinWebViewFactory.create(mContext, new WindowAndroid(mContext));

        mSheetContentView = new FrameLayout(mContext);
        mSheetContentView.addView(mThinWebView.getView());

        int topPadding =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        mSheetContentView.setPadding(0, topPadding, 0, 0);
    }

    private void createToolbarView() {
        mToolbarView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.ephemeral_tab_toolbar, null);
    }

    public void setTitleText(String text) {
        TextView toolbarText = mToolbarView.findViewById(R.id.ephemeral_tab_text);
        toolbarText.setText(text);
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    @Override
    public void destroy() {
        mThinWebView.destroy();
    }

    @Override
    public int getPriority() {
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean isPeekStateEnabled() {
        return true;
    }

    @Override
    public boolean wrapContentEnabled() {
        return true;
    }

    // TODO(shaktisahu): Provide correct strings for the following methods.
    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }
}

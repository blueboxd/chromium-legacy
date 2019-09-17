// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.thinwebview.ThinWebView;
import org.chromium.chrome.browser.thinwebview.ThinWebViewFactory;
import org.chromium.chrome.browser.ui.widget.FadingShadow;
import org.chromium.chrome.browser.ui.widget.FadingShadowView;
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
    private final Runnable mOpenNewTabCallback;
    private final Runnable mToolbarClickCallback;

    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;

    private WebContents mWebContents;
    private ContentView mWebContentView;
    private ThinWebView mThinWebView;
    private FadingShadowView mShadow;
    private Drawable mCurrentFavicon;
    private ImageView mFaviconView;

    /**
     * Constructor.
     * @param context An Android context.
     * @param openNewTabCallback Callback invoked to open a new tab.
     * @param toolbarClickCallback Callback invoked when user clicks on the toolbar.
     */
    public EphemeralTabSheetContent(
            Context context, Runnable openNewTabCallback, Runnable toolbarClickCallback) {
        mContext = context;
        mOpenNewTabCallback = openNewTabCallback;
        mToolbarClickCallback = toolbarClickCallback;

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

    /**
     * Create a ThinWebView, add it to the view hierarchy, which represents the contents of the
     * bottom sheet.
     */
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
        mShadow = mToolbarView.findViewById(R.id.shadow);
        mShadow.init(ApiCompatibilityUtils.getColor(
                             mContext.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);
        ImageView openInNewTabButton = mToolbarView.findViewById(R.id.open_in_new_tab);
        openInNewTabButton.setOnClickListener(view -> mOpenNewTabCallback.run());

        View toolbar = mToolbarView.findViewById(R.id.toolbar);
        toolbar.setOnClickListener(view -> mToolbarClickCallback.run());

        mFaviconView = mToolbarView.findViewById(R.id.favicon);
        mCurrentFavicon = mFaviconView.getDrawable();
    }

    /** Method to be called to start the favicon anmiation. */
    public void startFaviconAnimation(Drawable favicon) {
        assert favicon != null;

        // TODO(shaktisahu): Find out if there is a better way for this animation.
        Drawable presentedDrawable = favicon;
        if (mCurrentFavicon != null && !(mCurrentFavicon instanceof TransitionDrawable)) {
            TransitionDrawable transitionDrawable = ApiCompatibilityUtils.createTransitionDrawable(
                    new Drawable[] {mCurrentFavicon, favicon});
            transitionDrawable.setCrossFadeEnabled(true);
            transitionDrawable.startTransition((int) BottomSheet.BASE_ANIMATION_DURATION_MS);
            presentedDrawable = transitionDrawable;
        }

        mFaviconView.setImageDrawable(presentedDrawable);
        mCurrentFavicon = favicon;
    }

    /** Sets the ephemeral tab title text. */
    public void setTitleText(String text) {
        TextView toolbarText = mToolbarView.findViewById(R.id.ephemeral_tab_text);
        toolbarText.setText(text);
    }

    /** Sets the progress percentage on the progress bar. */
    public void setProgress(int progress) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setProgress(progress);
    }

    /** Called to show or hide the progress bar. */
    public void setProgressVisible(boolean visible) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setVisibility(visible ? View.VISIBLE : View.GONE);
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
    public boolean hideOnScroll() {
        return false;
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.os.Handler;
import android.view.View;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandlerToolbar mediator, which is responsible for receiving events from the view and
 * notifies the backend (the coordinator).
 */
/* package */ class PaymentHandlerToolbarMediator
        extends WebContentsObserver implements View.OnClickListener {
    // Abbreviated for the length limit.
    private static final String TAG = "PaymentHandlerTb";
    /** The delay (four video frames - for 60Hz) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;
    /**
     * The minimum load progress that can be shown when a page is loading.  This is not 0 so that
     * it's obvious to the user that something is attempting to load.
     */
    /* package */ static final float MINIMUM_LOAD_PROGRESS = 0.05f;

    private final PropertyModel mModel;
    /** The handler to delay hiding the progress bar. */
    private Handler mHideProgressBarHandler;
    /** Postfixed with "Ref" to distinguish from mWebContent in WebContentsObserver. */
    private final WebContents mWebContentsRef;
    private final boolean mIsSmallDevice;
    private final ChromeActivity mChromeActivity;

    /**
     * Build a new mediator that handle events from outside the payment handler toolbar component.
     * @param model The {@link PaymentHandlerToolbarProperties} that holds all the view state for
     *         the payment handler toolbar component.
     * @param chromeActivity The {@link ChromeActivity}.
     * @param webContents The web-contents that loads the payment app.
     * @param isSmallDevice Whether the device screen is considered small.
     */
    /* package */ PaymentHandlerToolbarMediator(PropertyModel model, ChromeActivity chromeActivity,
            WebContents webContents, boolean isSmallDevice) {
        super(webContents);
        mIsSmallDevice = isSmallDevice;
        mWebContentsRef = webContents;
        mModel = model;
        mChromeActivity = chromeActivity;
    }

    // WebContentsObserver:
    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        // Hides the Progress Bar after a delay to make sure it is rendered for at least
        // a few frames, otherwise its completion won't be visually noticeable.
        mHideProgressBarHandler = new Handler();
        mHideProgressBarHandler.postDelayed(() -> {
            mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
            mHideProgressBarHandler = null;
        }, HIDE_PROGRESS_BAR_DELAY_MS);
        return;
    }

    @Override
    public void didFailLoad(boolean isMainFrame, int errorCode, String failingUrl) {
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
    }

    @Override
    public void didFinishNavigation(NavigationHandle navigation) {
        if (!navigation.hasCommitted() || !navigation.isInMainFrame()) return;
        mModel.set(PaymentHandlerToolbarProperties.URL, mWebContentsRef.getVisibleUrl());
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
    }

    @Override
    public void didStartNavigation(NavigationHandle navigation) {
        if (!navigation.isInMainFrame() || navigation.isSameDocument()) return;
        mModel.set(PaymentHandlerToolbarProperties.SECURITY_ICON,
                getSecurityIconResource(ConnectionSecurityLevel.NONE));
    }

    @Override
    public void titleWasSet(String title) {
        mModel.set(PaymentHandlerToolbarProperties.TITLE, title);
    }

    @Override
    public void loadProgressChanged(float progress) {
        assert progress <= 1.0;
        if (progress == 1.0) return;
        // If the load restarts when the progress bar is waiting to hide, cancel the handler
        // callbacks.
        if (mHideProgressBarHandler != null) {
            mHideProgressBarHandler.removeCallbacksAndMessages(null);
            mHideProgressBarHandler = null;
        }
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true);
        mModel.set(PaymentHandlerToolbarProperties.LOAD_PROGRESS,
                Math.max(progress, MINIMUM_LOAD_PROGRESS));
    }

    @DrawableRes
    private int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        return SecurityStatusIcon.getSecurityIconResource(securityLevel,
                SecurityStateModel.shouldShowDangerTriangleForWarningLevel(), mIsSmallDevice,
                /*skipIconForNeutralState=*/true);
    }

    @Override
    public void didChangeVisibleSecurityState() {
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContentsRef);
        mModel.set(PaymentHandlerToolbarProperties.SECURITY_ICON,
                getSecurityIconResource(securityLevel));
    }

    // (PaymentHandlerToolbarView security icon's) OnClickListener:
    @Override
    public void onClick(View view) {
        if (mChromeActivity == null) return;
        PageInfoController.show(mChromeActivity, mWebContentsRef, null,
                PageInfoController.OpenedFromSource.TOOLBAR,
                /*offlinePageLoadUrlDelegate=*/
                new OfflinePageUtils.WebContentsOfflinePageLoadUrlDelegate(mWebContentsRef));
    };
}

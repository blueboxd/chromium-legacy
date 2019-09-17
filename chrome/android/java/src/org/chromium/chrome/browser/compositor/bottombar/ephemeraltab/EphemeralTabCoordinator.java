// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.net.MalformedURLException;
import java.net.URL;

/**
 * Central class for ephemeral tab, responsible for spinning off other classes necessary to display
 * the preview tab UI.
 */
public class EphemeralTabCoordinator {
    /** The delay (four video frames) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;

    // TODO(crbug/1001256): Use Context after removing dependency on OverlayPanelContent.
    private final ChromeActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final FaviconLoader mFaviconLoader;
    private OverlayPanelContent mPanelContent;
    private EphemeralTabSheetContent mSheetContent;
    private boolean mIsIncognito;
    private String mUrl;

    /**
     * Constructor.
     * @param activity The associated {@link ChromeActivity}.
     * @param bottomSheetController The associated {@link BottomSheetController}.
     */
    public EphemeralTabCoordinator(
            ChromeActivity activity, BottomSheetController bottomSheetController) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;
        mFaviconLoader = new FaviconLoader(mActivity);
        mBottomSheetController.getBottomSheet().addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheet.SheetState.HIDDEN) destroyContent();
            }
        });
    }

    /**
     * Entry point for ephemeral tab flow. This will create an ephemeral tab and show it in the
     * bottom sheet.
     * @param url The URL to be shown.
     * @param title The title to be shown.
     * @param isIncognito Whether we are currently in incognito mode.
     */
    public void requestOpenSheet(String url, String title, boolean isIncognito) {
        mUrl = url;
        mIsIncognito = isIncognito;
        if (mSheetContent == null) {
            mSheetContent = new EphemeralTabSheetContent(
                    mActivity, () -> openInNewTab(), () -> onToolbarClick());
        }

        getContent().loadUrl(url, true);
        getContent().updateBrowserControlsState(true);
        mSheetContent.attachWebContents(
                getContent().getWebContents(), (ContentView) getContent().getContainerView());
        mSheetContent.setTitleText(title);
        mBottomSheetController.requestShowContent(mSheetContent, true);

        // TODO(donnd): Collect UMA with OverlayPanel.StateChangeReason.CLICK.
    }

    private OverlayPanelContent getContent() {
        if (mPanelContent == null) {
            mPanelContent = new OverlayPanelContent(new EphemeralTabPanelContentDelegate(),
                    new PageLoadProgressObserver(), mActivity, mIsIncognito,
                    mActivity.getResources().getDimensionPixelSize(
                            R.dimen.toolbar_height_no_shadow));
        }
        return mPanelContent;
    }

    private void destroyContent() {
        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }

        if (mPanelContent != null) {
            mPanelContent.destroy();
            mPanelContent = null;
        }
    }

    private void openInNewTab() {
        if (canPromoteToNewTab() && mUrl != null) {
            mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
            mActivity.getCurrentTabCreator().createNewTab(
                    new LoadUrlParams(mUrl, PageTransition.LINK), TabLaunchType.FROM_LINK,
                    mActivity.getActivityTabProvider().get());
        }
    }

    private void onToolbarClick() {
        if (mBottomSheetController.getBottomSheet().getSheetState()
                == BottomSheet.SheetState.PEEK) {
            mBottomSheetController.expandSheet();
        }
    }

    private boolean canPromoteToNewTab() {
        return !mActivity.isCustomTab();
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (mSheetContent == null) return;
        mSheetContent.startFaviconAnimation(drawable);
    }

    /**
     * Observes the ephemeral tab web contents and loads the associated favicon.
     */
    private class EphemeralTabPanelContentDelegate extends OverlayContentDelegate {
        private String mCurrentUrl;

        @Override
        public void onMainFrameLoadStarted(String url, boolean isExternalUrl) {
            try {
                String newHost = new URL(url).getHost();
                String curHost = mCurrentUrl == null ? null : new URL(mCurrentUrl).getHost();
                if (!newHost.equals(curHost)) {
                    mCurrentUrl = url;
                    mFaviconLoader.loadFavicon(url, (drawable) -> onFaviconAvailable(drawable));
                }
            } catch (MalformedURLException e) {
                assert false : "Malformed URL should not be passed.";
            }
        }
    }

    /** Observes the ephemeral tab page load progress and updates the progress bar. */
    private class PageLoadProgressObserver extends OverlayContentProgressObserver {
        @Override
        public void onProgressBarStarted() {
            if (mSheetContent == null) return;
            mSheetContent.setProgressVisible(true);
            mSheetContent.setProgress(0);
        }

        @Override
        public void onProgressBarUpdated(int progress) {
            if (mSheetContent == null) return;
            mSheetContent.setProgress(progress);
        }

        @Override
        public void onProgressBarFinished() {
            // Hides the Progress Bar after a delay to make sure it is rendered for at least
            // a few frames, otherwise its completion won't be visually noticeable.
            new Handler().postDelayed(() -> {
                if (mSheetContent == null) return;
                mSheetContent.setProgressVisible(false);
            }, HIDE_PROGRESS_BAR_DELAY_MS);
        }
    }

    /**
     * Helper class to generate a favicon for a given URL and resize it to the desired dimensions
     * for displaying it on the image view.
     */
    private static class FaviconLoader {
        private final Context mContext;
        private final FaviconHelper mFaviconHelper;
        private final FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
        private final RoundedIconGenerator mIconGenerator;
        private final int mFaviconSize;

        /** Constructor. */
        public FaviconLoader(Context context) {
            mContext = context;
            mFaviconHelper = new FaviconHelper();
            mDefaultFaviconHelper = new FaviconHelper.DefaultFaviconHelper();
            mIconGenerator =
                    ViewUtils.createDefaultRoundedIconGenerator(mContext.getResources(), true);
            mFaviconSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.preview_tab_favicon_size);
        }

        /**
         * Generates a favicon for a given URL. If no favicon was could be found or generated from
         * the URL, a default favicon will be shown.
         * @param url The URL for which favicon is to be generated.
         * @param callback The callback to be invoked to display the final image.
         */
        public void loadFavicon(final String url, Callback<Drawable> callback) {
            FaviconHelper.FaviconImageCallback imageCallback = (bitmap, iconUrl) -> {
                Drawable drawable = faviconDrawable(bitmap, url);
                if (drawable == null) {
                    drawable = mDefaultFaviconHelper.getDefaultFaviconDrawable(mContext, url, true);
                }

                callback.onResult(drawable);
            };

            mFaviconHelper.getLocalFaviconImageForURL(
                    Profile.getLastUsedProfile(), url, mFaviconSize, imageCallback);
        }

        /**
         * Generates a rounded bitmap for the given favicon. If the given favicon is null, generates
         * a favicon from the URL instead.
         */
        private Drawable faviconDrawable(Bitmap image, String url) {
            if (url == null) return null;
            if (image == null) {
                image = mIconGenerator.generateIconForUrl(url);
                return new BitmapDrawable(mContext.getResources(),
                        Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, true));
            }

            return ViewUtils.createRoundedBitmapDrawable(
                    Bitmap.createScaledBitmap(image, mFaviconSize, mFaviconSize, true),
                    ViewUtils.DEFAULT_FAVICON_CORNER_RADIUS);
        }
    }
}

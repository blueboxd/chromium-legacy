// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Central class for ephemeral tab, responsible for spinning off other classes necessary to display
 * the preview tab UI.
 */
public class EphemeralTabCoordinator implements View.OnLayoutChangeListener {
    private final Context mContext;
    private final WindowAndroid mWindow;
    private final View mLayoutView;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<TabCreator> mTabCreator;
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final EphemeralTabMetrics mMetrics = new EphemeralTabMetrics();
    private final Supplier<Boolean> mCanPromoteToNewTab;

    private EphemeralTabMediator mMediator;

    private WebContents mWebContents;
    private ContentView mContentView;
    private EphemeralTabSheetContent mSheetContent;
    private EmptyBottomSheetObserver mSheetObserver;

    private String mUrl;
    private int mCurrentMaxSheetHeight;
    private Profile mProfile;
    private boolean mOpened;

    /**
     * Constructor.
     * @param context The associated {@link Context}.
     * @param window The associated {@link WindowAndroid}.
     * @param layoutView The {@link View} to listen layout change on.
     * @param tabProvider Supplier for the current activity tab.
     * @param tabCreator Supplier for {@link TabCreator} handling a new tab creation.
     * @param bottomSheetController Supplier for The associated {@link BottomSheetController}.
     * @param canPromoteToNewTab A predicate tells if the tab can be promoted to a normal tab.
     */
    public EphemeralTabCoordinator(Context context, WindowAndroid window, View layoutView,
            Supplier<Tab> tabProvider, Supplier<TabCreator> tabCreator,
            Supplier<BottomSheetController> bottomSheetController,
            Supplier<Boolean> canPromoteToNewTab) {
        mContext = context;
        mWindow = window;
        mLayoutView = layoutView;
        mTabProvider = tabProvider;
        mTabCreator = tabCreator;
        mBottomSheetController = bottomSheetController;
        mCanPromoteToNewTab = canPromoteToNewTab;
    }

    /**
     * Checks if this feature (a.k.a. "Preview page/image") is supported.
     * @return {@code true} if the feature is enabled.
     */
    public static boolean isSupported() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * Checks if the preview tab is in open (peek) state.
     */
    public boolean isOpened() {
        return mOpened;
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
        Profile profile = isIncognito ? Profile.getLastUsedRegularProfile().getOffTheRecordProfile()
                                      : Profile.getLastUsedRegularProfile();

        if (mMediator == null) {
            float topControlsHeight =
                    mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                    / mWindow.getDisplay().getDipScale();
            mMediator = new EphemeralTabMediator(mBottomSheetController.get(),
                    new FaviconLoader(mContext), mMetrics, (int) topControlsHeight);
        }
        if (mWebContents == null) {
            assert mSheetContent == null;
            createWebContents(isIncognito);
            mSheetObserver = new EmptyBottomSheetObserver() {
                private int mCloseReason;

                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent != mSheetContent) {
                        destroyContent();
                    }
                }

                @Override
                public void onSheetStateChanged(int newState) {
                    if (mSheetContent == null) return;
                    switch (newState) {
                        case SheetState.PEEK:
                            mOpened = true;
                            mMetrics.recordMetricsForPeeked();
                            break;
                        case SheetState.FULL:
                            mMetrics.recordMetricsForOpened();
                            break;
                    }
                }

                @Override
                public void onSheetClosed(int reason) {
                    // "Closed" actually means "Peek" for bottom sheet. Save the reason to
                    // log when the sheet goes to hidden state.
                    mCloseReason = reason;
                    mOpened = false;
                }

                @Override
                public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                    if (mSheetContent == null) return;
                    if (heightFraction == 0.0f) mMetrics.recordMetricsForClosed(mCloseReason);
                    if (mCanPromoteToNewTab.get()) {
                        mSheetContent.showOpenInNewTabButton(heightFraction);
                    }
                }
            };
            mBottomSheetController.get().addObserver(mSheetObserver);
            mSheetContent = new EphemeralTabSheetContent(mContext, this::openInNewTab,
                    this::onToolbarClick, this::close, getMaxSheetHeight());
            mMediator.init(mWebContents, mContentView, mSheetContent, profile);
            mLayoutView.addOnLayoutChangeListener(this);
        }

        mMediator.requestShowContent(url, title);

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (tracker.isInitialized()) tracker.notifyEvent(EventConstants.EPHEMERAL_TAB_USED);
    }

    private void createWebContents(boolean incognito) {
        assert mWebContents == null;

        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mWebContents = WebContentsFactory.createWebContents(incognito, true);

        mContentView = ContentView.createContentView(mContext, mWebContents);

        mWebContents.initialize(ChromeVersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView), mContentView, mWindow,
                WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents);
    }

    private void destroyContent() {
        mSheetContent = null; // Will be destroyed by BottomSheet controller.

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mContentView = null;
        }

        if (mMediator != null) mMediator.destroyContent();

        mLayoutView.removeOnLayoutChangeListener(this);
        if (mSheetObserver != null) mBottomSheetController.get().removeObserver(mSheetObserver);
    }

    private void openInNewTab() {
        if (mCanPromoteToNewTab.get() && mUrl != null) {
            mBottomSheetController.get().hideContent(
                    mSheetContent, /* animate= */ true, StateChangeReason.PROMOTE_TAB);
            mTabCreator.get().createNewTab(new LoadUrlParams(mUrl, PageTransition.LINK),
                    TabLaunchType.FROM_LINK, mTabProvider.get());
            mMetrics.recordOpenInNewTab();
        }
    }

    private void onToolbarClick() {
        int state = mBottomSheetController.get().getSheetState();
        if (state == SheetState.PEEK) {
            mBottomSheetController.get().expandSheet();
        } else if (state == SheetState.FULL) {
            mBottomSheetController.get().collapseSheet(true);
        }
    }

    /**
     * Close the ephemeral tab.
     */
    public void close() {
        mBottomSheetController.get().hideContent(mSheetContent, /* animate= */ true);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mSheetContent == null) return;

        // It may not be possible to update the content height when the actual height changes
        // due to the current tab not being ready yet. Try it later again when the tab
        // (hence MaxSheetHeight) becomes valid.
        int maxSheetHeight = getMaxSheetHeight();
        if (maxSheetHeight == 0 || mCurrentMaxSheetHeight == maxSheetHeight) return;
        mSheetContent.updateContentHeight(maxSheetHeight);
        mCurrentMaxSheetHeight = maxSheetHeight;
    }

    private int getMaxSheetHeight() {
        Tab tab = mTabProvider.get();
        if (tab == null || tab.getView() == null) return 0;
        return (int) (tab.getView().getHeight() * 0.9f);
    }

    /**
     * Helper class to generate a favicon for a given URL and resize it to the desired dimensions
     * for displaying it on the image view.
     */
    static class FaviconLoader {
        private final Context mContext;
        private final FaviconHelper mFaviconHelper;
        private final RoundedIconGenerator mIconGenerator;
        private final int mFaviconSize;

        /** Constructor. */
        public FaviconLoader(Context context) {
            mContext = context;
            mFaviconHelper = new FaviconHelper();
            mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext.getResources());
            mFaviconSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.preview_tab_favicon_size);
        }

        /**
         * Generates a favicon for a given URL. If no favicon was could be found or generated from
         * the URL, a default favicon will be shown.
         * @param url The URL for which favicon is to be generated.
         * @param callback The callback to be invoked to display the final image.
         * @param profile The profile for which favicon service is used.
         */
        public void loadFavicon(final String url, Callback<Drawable> callback, Profile profile) {
            assert profile != null;
            FaviconHelper.FaviconImageCallback imageCallback = (bitmap, iconUrl) -> {
                Drawable drawable;
                if (bitmap != null) {
                    drawable = FaviconUtils.createRoundedBitmapDrawable(
                            mContext.getResources(), bitmap);
                } else {
                    drawable = UiUtils.getTintedDrawable(mContext, R.drawable.ic_globe_24dp,
                            R.color.default_icon_color_tint_list);
                }

                callback.onResult(drawable);
            };

            mFaviconHelper.getLocalFaviconImageForURL(profile, url, mFaviconSize, imageCallback);
        }
    }
}

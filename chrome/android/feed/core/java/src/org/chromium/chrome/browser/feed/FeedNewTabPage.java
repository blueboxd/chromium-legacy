// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ScrollView;

import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.api.client.scope.StreamScope;
import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.NonDismissibleHeader;
import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.SnackbarCallbackApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.feed.tooltip.BasicTooltipApi;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationLayout;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.widget.displaystyle.ViewResizer;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends NewTabPage {
    private final FeedNewTabPageMediator mMediator;
    private final int mDefaultMargin;
    private final int mWideMargin;

    private UiConfig mUiConfig;
    private HistoryNavigationLayout mRootView;
    private ContextMenuManager mContextMenuManager;

    // Used when Feed is enabled.
    private @Nullable Stream mStream;
    private @Nullable FeedImageLoader mImageLoader;
    private @Nullable StreamLifecycleManager mStreamLifecycleManager;
    private @Nullable SectionHeaderView mSectionHeaderView;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;
    private @Nullable ViewResizer mStreamViewResizer;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;
    private @Nullable ViewResizer mScrollViewResizer;

    private static class BasicSnackbarApi implements SnackbarApi {
        private final SnackbarManager mManager;

        public BasicSnackbarApi(SnackbarManager manager) {
            mManager = manager;
        }

        @Override
        public void show(String message) {
            mManager.showSnackbar(Snackbar.make(message,
                    new SnackbarManager.SnackbarController() {}, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_FEED_NTP_STREAM));
        }

        @Override
        public void show(String message, String action, SnackbarCallbackApi callback) {
            mManager.showSnackbar(
                    Snackbar.make(message,
                                    new SnackbarManager.SnackbarController() {
                                        @Override
                                        public void onAction(Object actionData) {
                                            callback.onDismissedWithAction();
                                        }

                                        @Override
                                        public void onDismissNoAction(Object actionData) {
                                            callback.onDismissNoAction();
                                        }
                                    },
                                    Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM)
                            .setAction(action, null));
        }
    }

    private static class BasicStreamConfiguration implements StreamConfiguration {
        public BasicStreamConfiguration() {}

        @Override
        public int getPaddingStart() {
            return 0;
        }
        @Override
        public int getPaddingEnd() {
            return 0;
        }
        @Override
        public int getPaddingTop() {
            return 0;
        }
        @Override
        public int getPaddingBottom() {
            return 0;
        }
    }

    private static class BasicCardConfiguration implements CardConfiguration {
        private final Resources mResources;
        private final UiConfig mUiConfig;
        private final int mCornerRadius;
        private final int mCardMargin;
        private final int mCardWideMargin;

        public BasicCardConfiguration(Resources resources, UiConfig uiConfig) {
            mResources = resources;
            mUiConfig = uiConfig;
            mCornerRadius = mResources.getDimensionPixelSize(R.dimen.default_rounded_corner_radius);
            mCardMargin = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_margin);
            mCardWideMargin =
                    mResources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        }

        @Override
        public int getDefaultCornerRadius() {
            return mCornerRadius;
        }

        @Override
        public Drawable getCardBackground() {
            return ApiCompatibilityUtils.getDrawable(mResources,
                    FeedConfiguration.getFeedUiEnabled()
                            ? R.drawable.hairline_border_card_background_with_inset
                            : R.drawable.hairline_border_card_background);
        }

        @Override
        public int getCardBottomMargin() {
            return mCardMargin;
        }

        @Override
        public int getCardStartMargin() {
            return 0;
        }

        @Override
        public int getCardEndMargin() {
            return 0;
        }
    }

    private class SignInPromoHeader implements Header {
        @Override
        public View getView() {
            return getSigninPromoView();
        }

        @Override
        public boolean isDismissible() {
            return true;
        }

        @Override
        public void onDismissed() {
            mMediator.onSignInPromoDismissed();
        }
    }

    /**
     * Provides the additional capabilities needed for the {@link FeedNewTabPage} container view.
     */
    private class RootView extends HistoryNavigationLayout {
        /**
         * @param context The context of the application.
         * @param constructedTimeNs The timestamp at which the new tab page's construction started.
         */
        public RootView(Context context, long constructedTimeNs) {
            super(context);
            NewTabPageUma.trackTimeToFirstDraw(this, constructedTimeNs);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            if (super.onInterceptTouchEvent(ev)) return true;
            if (mMediator != null && !mMediator.getTouchEnabled()) return true;

            return !(mTab != null && DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroid()))
                    && (mFakeboxDelegate != null && mFakeboxDelegate.isUrlBarFocused());
        }

        @Override
        public boolean wasLastSideSwipeGestureConsumed() {
            return mStream.willHandleHorizontalSwipe();
        }
    }

    /**
     * Provides the additional capabilities needed for the {@link ScrollView}.
     */
    private class PolicyScrollView extends ScrollView {
        public PolicyScrollView(Context context) {
            super(context);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }
    }

    /**
     * Constructs a new FeedNewTabPage.
     *
     * @param activity The containing {@link ChromeActivity}.
     * @param nativePageHost The host for this native page.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param activityTabProvider Allows us to check if we are the current tab.
     * @param activityLifecycleDispatcher Allows us to subscribe to backgrounding events.
     */
    public FeedNewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector, SigninManager signinManager,
            ActivityTabProvider activityTabProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        super(activity, nativePageHost, tabModelSelector, activityTabProvider,
                activityLifecycleDispatcher);

        Resources resources = activity.getResources();
        mDefaultMargin =
                resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        mWideMargin = resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        LayoutInflater inflater = LayoutInflater.from(activity);
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);

        // Mediator should be created before any Stream changes.
        mMediator = new FeedNewTabPageMediator(
                this, new SnapScrollHelper(mNewTabPageManager, mNewTabPageLayout), signinManager);

        // Don't store a direct reference to the activity, because it might change later if the tab
        // is reparented.
        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = () -> mTab.getActivity().closeContextMenu();
        mContextMenuManager = new ContextMenuManager(mNewTabPageManager.getNavigationDelegate(),
                mMediator, closeContextMenuCallback, NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        mNewTabPageLayout.initialize(mNewTabPageManager, mTab, mTileGroupDelegate,
                mSearchProviderHasLogo,
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(), mMediator,
                mContextMenuManager, mUiConfig);
    }

    @Override
    protected void initializeMainView(Context context, NativePageHost host) {
        int topPadding = context.getResources().getDimensionPixelOffset(R.dimen.tab_strip_height);

        mRootView = new RootView(context, mConstructedTimeNs);
        mRootView.setPadding(0, topPadding, 0, 0);
        mRootView.setNavigationDelegate(host.createHistoryNavigationDelegate());
        mUiConfig = new UiConfig(mRootView);
    }

    @Override
    public void destroy() {
        super.destroy();
        mMediator.destroy();
        if (mStreamLifecycleManager != null) mStreamLifecycleManager.destroy();
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        if (mImageLoader != null) mImageLoader.destroy();
        mImageLoader = null;
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    protected void saveLastScrollPosition() {
        // This behavior is handled by the StreamLifecycleManager and the Feed library.
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageLayout.shouldCaptureThumbnail() || mMediator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        ViewUtils.captureBitmap(mRootView, canvas);
        mMediator.onThumbnailCaptured();
    }

    /**
     * @return The {@link StreamLifecycleManager} that manages the lifecycle of the {@link Stream}.
     */
    StreamLifecycleManager getStreamLifecycleManager() {
        return mStreamLifecycleManager;
    }

    /** @return The {@link Stream} that this class holds. */
    Stream getStream() {
        return mStream;
    }

    /**
     * Create a {@link Stream} for this class.
     */
    void createStream() {
        if (mScrollViewForPolicy != null) {
            mRootView.removeView(mScrollViewForPolicy);
            mScrollViewForPolicy = null;
            mScrollViewResizer.detach();
            mScrollViewResizer = null;
        }

        ProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
        assert feedProcessScope != null;

        FeedAppLifecycle appLifecycle = FeedProcessScopeFactory.getFeedAppLifecycle();
        appLifecycle.onNTPOpened();

        ChromeActivity chromeActivity = mTab.getActivity();
        Profile profile = mTab.getProfile();

        mImageLoader = new FeedImageLoader(
                chromeActivity, GlobalDiscardableReferencePool.getReferencePool());
        FeedLoggingBridge loggingBridge = FeedProcessScopeFactory.getFeedLoggingBridge();
        FeedOfflineIndicator offlineIndicator = FeedProcessScopeFactory.getFeedOfflineIndicator();
        Runnable consumptionObserver = () -> {
            FeedScheduler scheduler = FeedProcessScopeFactory.getFeedScheduler();
            if (scheduler != null) {
                scheduler.onSuggestionConsumed();
            }
        };
        ActionApi actionApi = new FeedActionHandler(mNewTabPageManager.getNavigationDelegate(),
                consumptionObserver, offlineIndicator, OfflinePageBridge.getForProfile(profile),
                loggingBridge);

        TooltipApi tooltipApi = new BasicTooltipApi();

        StreamScope streamScope =
                feedProcessScope
                        .createStreamScopeBuilder(chromeActivity, mImageLoader, actionApi,
                                new BasicStreamConfiguration(),
                                new BasicCardConfiguration(
                                        chromeActivity.getResources(), mUiConfig),
                                new BasicSnackbarApi(mNewTabPageManager.getSnackbarManager()),
                                offlineIndicator, tooltipApi)
                        .setIsBackgroundDark(
                                chromeActivity.getNightModeStateProvider().isInNightMode())
                        .build();

        mStream = streamScope.getStream();
        mStreamLifecycleManager = new StreamLifecycleManager(mStream, chromeActivity, mTab);

        LayoutInflater inflater = LayoutInflater.from(chromeActivity);
        mSectionHeaderView = (SectionHeaderView) inflater.inflate(
                R.layout.new_tab_page_snippets_expandable_header, mRootView, false);

        View view = mStream.getView();
        view.setBackgroundResource(R.color.modern_primary_color);
        mRootView.addView(view);
        mStreamViewResizer =
                ViewResizer.createAndAttach(view, mUiConfig, mDefaultMargin, mWideMargin);

        UiUtils.removeViewFromParent(mNewTabPageLayout);
        UiUtils.removeViewFromParent(mSectionHeaderView);
        if (mSigninPromoView != null) UiUtils.removeViewFromParent(mSigninPromoView);
        mStream.setHeaderViews(Arrays.asList(new NonDismissibleHeader(mNewTabPageLayout),
                new NonDismissibleHeader(mSectionHeaderView)));
        mStream.addScrollListener(new FeedLoggingBridge.ScrollEventReporter(loggingBridge));

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            view.setDefaultFocusHighlightEnabled(false);
        }

        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // the scroll container for policy is removed.
        view.requestFocus();
    }

    /**
     * @return The {@link ScrollView} for displaying content for supervised user or enterprise
     *         policy.
     */
    ScrollView getScrollViewForPolicy() {
        return mScrollViewForPolicy;
    }

    /**
     * Create a {@link ScrollView} for displaying content for supervised user or enterprise policy.
     */
    void createScrollViewForPolicy() {
        if (mStream != null) {
            mStreamViewResizer.detach();
            mStreamViewResizer = null;
            mRootView.removeView(mStream.getView());
            assert mStreamLifecycleManager
                    != null
                : "StreamLifecycleManager should not be null when the Stream is not null.";
            mStreamLifecycleManager.destroy();
            mStreamLifecycleManager = null;
            // Do not call mStream.onDestroy(), the mStreamLifecycleManager has done that for us.
            mStream = null;
            mSectionHeaderView = null;
            mSigninPromoView = null;
            if (mImageLoader != null) {
                mImageLoader.destroy();
                mImageLoader = null;
            }
        }

        mScrollViewForPolicy = new PolicyScrollView(mTab.getActivity());
        mScrollViewForPolicy.setBackgroundColor(Color.WHITE);
        mScrollViewForPolicy.setVerticalScrollBarEnabled(false);

        // Make scroll view focusable so that it is the next focusable view when the url bar clears
        // focus.
        mScrollViewForPolicy.setFocusable(true);
        mScrollViewForPolicy.setFocusableInTouchMode(true);
        mScrollViewForPolicy.setContentDescription(
                mScrollViewForPolicy.getResources().getString(R.string.accessibility_new_tab_page));

        UiUtils.removeViewFromParent(mNewTabPageLayout);
        mScrollViewForPolicy.addView(mNewTabPageLayout);
        mRootView.addView(mScrollViewForPolicy);
        mScrollViewResizer = ViewResizer.createAndAttach(
                mScrollViewForPolicy, mUiConfig, mDefaultMargin, mWideMargin);
        mScrollViewForPolicy.requestFocus();
    }

    /** @return The {@link SectionHeaderView} for the Feed section header. */
    SectionHeaderView getSectionHeaderView() {
        return mSectionHeaderView;
    }

    /** @return The {@link PersonalizedSigninPromoView} for this class. */
    PersonalizedSigninPromoView getSigninPromoView() {
        if (mSigninPromoView == null) {
            LayoutInflater inflater = LayoutInflater.from(mRootView.getContext());
            mSigninPromoView = (PersonalizedSigninPromoView) inflater.inflate(
                    R.layout.personalized_signin_promo_view_modern_content_suggestions, mRootView,
                    false);
        }
        return mSigninPromoView;
    }

    /** Update header views in the Stream. */
    void updateHeaderViews(boolean isPromoVisible) {
        mStream.setHeaderViews(
                isPromoVisible ? Arrays.asList(new NonDismissibleHeader(mNewTabPageLayout),
                        new NonDismissibleHeader(mSectionHeaderView), new SignInPromoHeader())
                               : Arrays.asList(new NonDismissibleHeader(mNewTabPageLayout),
                                       new NonDismissibleHeader(mSectionHeaderView)));
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return false;
    }

    @VisibleForTesting
    FeedNewTabPageMediator getMediatorForTesting() {
        return mMediator;
    }

    @Override
    public View getSignInPromoViewForTesting() {
        return getSigninPromoView();
    }

    @Override
    public View getSectionHeaderViewForTesting() {
        return getSectionHeaderView();
    }
}

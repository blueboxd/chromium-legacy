// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage
        extends NewTabPage implements FeedSurfaceCoordinator.FeedSurfaceDelegate {
    private final ContextMenuManager mContextMenuManager;
    private FeedSurfaceCoordinator mCoordinator;

    /**
     * Constructs a new {@link FeedNewTabPage}.
     *
     * @param activity The containing {@link Activity}.
     * @param fullscreenManager {@link ChromeFullscreenManager} to observe for offset changes.
     * @param activityTabProvider Provides the current active tab.
     * @param overviewModeBehavior Overview mode to observe for mode changes.
     * @param snackbarManager {@link SnackBarManager} object.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param isTablet {@code true} if running on a Tablet device.
     * @param uma {@link NewTabPageUma} object recording user metrics.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param nativePageHost The host for this native page.
     * @param tab The {@link Tab} that contains this new tab page.
     */
    public FeedNewTabPage(Activity activity, ChromeFullscreenManager fullscreenManager,
            Supplier<Tab> activityTabProvider, @Nullable OverviewModeBehavior overviewModeBehavior,
            SnackbarManager snackbarManager, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector, boolean isTablet, NewTabPageUma uma,
            boolean isInNightMode, NativePageHost nativePageHost, Tab tab) {
        super(activity, fullscreenManager, activityTabProvider, overviewModeBehavior,
                snackbarManager, lifecycleDispatcher, tabModelSelector, isTablet, uma,
                isInNightMode, nativePageHost, tab);

        // TODO(twellington): Move this somewhere it can be shared with NewTabPageView?
        Runnable closeContextMenuCallback = activity::closeContextMenu;
        mContextMenuManager = new ContextMenuManager(mNewTabPageManager.getNavigationDelegate(),
                mCoordinator.getTouchEnabledDelegate(), closeContextMenuCallback,
                NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        mNewTabPageLayout.initialize(mNewTabPageManager, activity, mTileGroupDelegate,
                mSearchProviderHasLogo,
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                mCoordinator.getScrollDelegate(), mContextMenuManager, mCoordinator.getUiConfig(),
                activityTabProvider, lifecycleDispatcher, overviewModeBehavior, uma);
    }

    @Override
    protected void initializeMainView(Activity activity, NativePageHost host,
            Supplier<Tab> tabProvider, ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager, @Nullable OverviewModeBehavior overviewModeBehavior,
            TabModelSelector tabModelSelector, NewTabPageUma uma, boolean isInNightMode) {
        Profile profile = Profile.fromWebContents(mTab.getWebContents());
        ActionApi actionApi = new FeedActionHandler(mNewTabPageManager.getNavigationDelegate(),
                FeedProcessScopeFactory.getFeedConsumptionObserver(),
                FeedProcessScopeFactory.getFeedOfflineIndicator(),
                OfflinePageBridge.getForProfile(profile),
                FeedProcessScopeFactory.getFeedLoggingBridge(), activity, profile);
        LayoutInflater inflater = LayoutInflater.from(activity);
        mNewTabPageLayout = (NewTabPageLayout) inflater.inflate(R.layout.new_tab_page_layout, null);
        SectionHeaderView sectionHeaderView = (SectionHeaderView) inflater.inflate(
                R.layout.new_tab_page_snippets_expandable_header, null, false);
        mCoordinator = new FeedSurfaceCoordinator(activity, snackbarManager, tabModelSelector,
                tabProvider, new SnapScrollHelper(mNewTabPageManager, mNewTabPageLayout),
                mNewTabPageLayout, sectionHeaderView, actionApi, isInNightMode, this);

        // Record the timestamp at which the new tab page's construction started.
        uma.trackTimeToFirstDraw(mCoordinator.getView(), mConstructedTimeNs);
    }

    @Override
    public void destroy() {
        super.destroy();
        mCoordinator.destroy();
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
    }

    @Override
    public View getView() {
        return mCoordinator.getView();
    }

    @Override
    protected void saveLastScrollPosition() {
        // This behavior is handled by the StreamLifecycleManager and the Feed library.
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageLayout.shouldCaptureThumbnail() || mCoordinator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageLayout.onPreCaptureThumbnail();
        mCoordinator.captureThumbnail(canvas);
    }

    // Implements FeedSurfaceDelegate
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new NtpStreamLifecycleManager(stream, activity, mTab);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return !(mTab != null && DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroid()))
                && (mFakeboxDelegate != null && mFakeboxDelegate.isUrlBarFocused());
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return false;
    }

    @VisibleForTesting
    FeedSurfaceCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    @VisibleForTesting
    FeedSurfaceMediator getMediatorForTesting() {
        return mCoordinator.getMediatorForTesting();
    }

    @VisibleForTesting
    public Stream getStreamForTesting() {
        return mCoordinator.getStream();
    }

    @Override
    public View getSignInPromoViewForTesting() {
        return mCoordinator.getSigninPromoView();
    }

    @Override
    public View getSectionHeaderViewForTesting() {
        return mCoordinator.getSectionHeaderView();
    }
}

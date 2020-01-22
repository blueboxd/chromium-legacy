// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

/**
 * Coordinator object for gesture navigation.
 */
public class HistoryNavigationCoordinator
        implements InsetObserverView.WindowInsetObserver, Destroyable {
    private CompositorViewHolder mCompositorViewHolder;
    private HistoryNavigationLayout mNavigationLayout;
    private InsetObserverView mInsetObserverView;
    private ActivityTabTabObserver mActivityTabObserver;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private Tab mTab;

    /**
     * Initializes the navigation layout and internal objects.
     * @param compositorViewHolder Parent view for navigation layout.
     * @param tabProvider Activity tab provider.
     * @param insetObserverView View that provides information about the inset and inset
     *        capabilities of the device.
     */
    public void init(ActivityLifecycleDispatcher lifecycleDispatcher,
            CompositorViewHolder compositorViewHolder, ActivityTabProvider tabProvider,
            InsetObserverView insetObserverView) {
        mNavigationLayout = new HistoryNavigationLayout(compositorViewHolder.getContext());
        mCompositorViewHolder = compositorViewHolder;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        lifecycleDispatcher.register(this);

        compositorViewHolder.addView(mNavigationLayout);
        compositorViewHolder.addTouchEventObserver(mNavigationLayout);
        mActivityTabObserver = new ActivityTabProvider.ActivityTabTabObserver(tabProvider) {
            @Override
            protected void onObservingDifferentTab(Tab tab) {
                if (mTab == tab) return;
                onTabSwitched(tab);
            }

            @Override
            public void onContentChanged(Tab tab) {
                initNavigationHandler(
                        tab, createDelegate(tab), tab.getWebContents(), tab.isNativePage());
            }

            @Override
            public void onDestroyed(Tab tab) {
                mTab = null;
            }
        };

        // We wouldn't hear about the first tab until the content changed or we switched tabs
        // if tabProvider.get() != null. Do here what we do when tab switching happens.
        if (tabProvider.get() != null) onTabSwitched(tabProvider.get());

        mNavigationLayout.setVisibility(View.INVISIBLE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mInsetObserverView = insetObserverView;
            insetObserverView.addObserver(this);
        }
    }

    /**
     * Creates {@link HistoryNavigationDelegate} for native/rendered pages on Tab.
     */
    private static HistoryNavigationDelegate createDelegate(Tab tab) {
        if (!isFeatureFlagEnabled() || ((TabImpl) tab).getActivity() == null) {
            return HistoryNavigationDelegate.DEFAULT;
        }

        return new HistoryNavigationDelegate() {
            // TODO(jinsukkim): Avoid getting activity from tab.
            private final Supplier<BottomSheetController> mController = () -> {
                ChromeActivity activity = ((TabImpl) tab).getActivity();
                return activity == null || activity.isActivityFinishingOrDestroyed()
                        ? null
                        : activity.getBottomSheetController();
            };

            @Override
            public NavigationHandler.ActionDelegate createActionDelegate() {
                return new TabbedActionDelegate(tab);
            }

            @Override
            public NavigationSheet.Delegate createSheetDelegate() {
                return new TabbedSheetDelegate(tab);
            }

            @Override
            public Supplier<BottomSheetController> getBottomSheetController() {
                return mController;
            }
        };
    }

    private static boolean isFeatureFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION);
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        // Resets navigation handler when the feature becomes disabled.
        if (!isNavigationEnabled(mCompositorViewHolder)) {
            initNavigationHandler(mTab, HistoryNavigationDelegate.DEFAULT, null, false);
        }
    }

    private void onTabSwitched(Tab tab) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        HistoryNavigationDelegate delegate =
                webContents != null && isNavigationEnabled(mCompositorViewHolder)
                ? createDelegate(tab)
                : HistoryNavigationDelegate.DEFAULT;

        // Also resets NavigationHandler when there's no tab (going into TabSwitcher).
        if (tab == null || webContents != null) {
            initNavigationHandler(
                    tab, delegate, webContents, tab != null ? tab.isNativePage() : false);
        }
    }

    /**
     * @param view {@link View} object to obtain the navigation setting from.
     * @return {@code true} if overscroll navigation is allowed to run on this page.
     */
    private static boolean isNavigationEnabled(View view) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return true;
        Insets insets = view.getRootWindowInsets().getSystemGestureInsets();
        return insets.left == 0 && insets.right == 0;
    }

    private void initNavigationHandler(Tab tab, HistoryNavigationDelegate delegate,
            WebContents webContents, boolean isNativePage) {
        assert mNavigationLayout != null;
        mNavigationLayout.initNavigationHandler(delegate, webContents, isNativePage);
        if (mTab != tab) {
            if (mTab != null) SwipeRefreshHandler.from(mTab).setNavigationHandler(null);
            if (tab != null) {
                SwipeRefreshHandler.from(tab).setNavigationHandler(
                        mNavigationLayout.getNavigationHandler());
            }
            mTab = tab;
        }
    }

    @Override
    public void onSafeAreaChanged(Rect area) {}

    @Override
    public void destroy() {
        if (mActivityTabObserver != null) {
            mActivityTabObserver.destroy();
            mActivityTabObserver = null;
        }
        if (mInsetObserverView != null) {
            mInsetObserverView.removeObserver(this);
            mInsetObserverView = null;
        }
        if (mCompositorViewHolder != null && mNavigationLayout != null) {
            mCompositorViewHolder.removeTouchEventObserver(mNavigationLayout);
            mCompositorViewHolder = null;
        }
        if (mNavigationLayout != null) {
            mNavigationLayout.destroy();
            mNavigationLayout = null;
        }
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
    }
}

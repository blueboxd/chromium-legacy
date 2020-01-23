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
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

/**
 * Coordinator object for gesture navigation.
 */
public class HistoryNavigationCoordinator implements InsetObserverView.WindowInsetObserver,
                                                     Destroyable, PauseResumeWithNativeObserver {
    private CompositorViewHolder mCompositorViewHolder;
    private HistoryNavigationLayout mNavigationLayout;
    private InsetObserverView mInsetObserverView;
    private ActivityTabTabObserver mActivityTabObserver;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private Tab mTab;
    private boolean mEnabled;

    /**
     * Creates the coordinator for gesture navigation and initializes internal objects.
     * @param lifecycleDispatcher Lifecycle dispatcher for the associated activity.
     * @param compositorViewHolder Parent view for navigation layout.
     * @param tabProvider Activity tab provider.
     * @param insetObserverView View that provides information about the inset and inset
     *        capabilities of the device.
     * @return HistoryNavigationCoordinator object or null if not enabled via feature flag.
     */
    public static HistoryNavigationCoordinator create(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CompositorViewHolder compositorViewHolder, ActivityTabProvider tabProvider,
            InsetObserverView insetObserverView) {
        if (!isFeatureFlagEnabled()) return null;
        HistoryNavigationCoordinator coordinator = new HistoryNavigationCoordinator();
        coordinator.init(lifecycleDispatcher, compositorViewHolder, tabProvider, insetObserverView);
        return coordinator;
    }

    /**
     * Initializes the navigation layout and internal objects.
     */
    private void init(ActivityLifecycleDispatcher lifecycleDispatcher,
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
                if (mTab != null && mTab.isInitialized()) {
                    SwipeRefreshHandler.from(mTab).setNavigationHandler(null);
                }
                mTab = tab;
                updateNavigationHandler();
            }

            @Override
            public void onContentChanged(Tab tab) {
                updateNavigationHandler();
            }

            @Override
            public void onDestroyed(Tab tab) {
                mTab = null;
            }
        };

        // We wouldn't hear about the first tab until the content changed or we switched tabs
        // if tabProvider.get() != null. Do here what we do when tab switching happens.
        if (tabProvider.get() != null) {
            mTab = tabProvider.get();
            onNavigationStateChanged();
        }

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
        if (((TabImpl) tab).getActivity() == null) return HistoryNavigationDelegate.DEFAULT;

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

    /**
     * @return {@code} true if the feature is enabled.
     */
    private boolean isFeatureEnabled() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return true;
        } else {
            assert mCompositorViewHolder.isAttachedToWindow();
            Insets insets = mCompositorViewHolder.getRootWindowInsets().getSystemGestureInsets();
            return insets.left == 0 && insets.right == 0;
        }
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        onNavigationStateChanged();
    }

    /**
     * Called when an event that can change the state of navigation feature. Update enabled status
     * and (re)initialize NavigationHandler if necessary.
     */
    private void onNavigationStateChanged() {
        boolean oldEnabled = mEnabled;
        mEnabled = isFeatureEnabled();
        if (mEnabled != oldEnabled) updateNavigationHandler();
    }

    /**
     * Initialize or reset {@link NavigationHandler} using the enabled state.
     */
    private void updateNavigationHandler() {
        if (mEnabled) {
            WebContents webContents = mTab != null ? mTab.getWebContents() : null;

            // Also updates NavigationHandler when tab == null (going into TabSwitcher).
            if (mTab == null || webContents != null) {
                HistoryNavigationDelegate delegate = webContents != null
                        ? createDelegate(mTab)
                        : HistoryNavigationDelegate.DEFAULT;
                boolean isNativePage = mTab != null ? mTab.isNativePage() : false;
                mNavigationLayout.initNavigationHandler(delegate, webContents, isNativePage);
            }
        } else {
            mNavigationLayout.destroy();
        }
        if (mTab != null) {
            SwipeRefreshHandler.from(mTab).setNavigationHandler(
                    mNavigationLayout.getNavigationHandler());
        }
    }

    @Override
    public void onSafeAreaChanged(Rect area) {}

    @Override
    public void onResumeWithNative() {
        // Check the enabled status again since the system gesture settings might have changed.
        // Post the task to work around wrong gesture insets returned from the framework.
        mNavigationLayout.post(this::onNavigationStateChanged);
    }

    @Override
    public void onPauseWithNative() {}

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

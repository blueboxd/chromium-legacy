// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.Nullable;

import org.chromium.base.ObservableSupplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

/** A UI coordinator the app menu. */
class AppMenuCoordinatorImpl implements AppMenuCoordinator {
    /**
     * Factory which creates the AppMenuHandlerImpl.
     */
    @VisibleForTesting
    public interface AppMenuHandlerFactory {
        /**
         * @param delegate Delegate used to check the desired AppMenu properties on show.
         * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
         * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the
         *         containing activity.
         * @param menuResourceId Resource Id that should be used as the source for the menu items.
         *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
         * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the
         *         containing activity.
         * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
         *         {@link OverviewModeBehavior} associated with the containing activity.
         * @return AppMenuHandlerImpl for the given activity and menu resource id.
         */
        AppMenuHandlerImpl get(AppMenuPropertiesDelegate delegate, AppMenuDelegate appMenuDelegate,
                int menuResourceId, View decorView,
                ActivityLifecycleDispatcher activityLifecycleDispatcher,
                @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier);
    }

    /**
     * @param factory The {@link AppMenuHandlerFactory} for creating {@link #mAppMenuHandler}
     */
    @VisibleForTesting
    public static void setAppMenuHandlerFactoryForTesting(AppMenuHandlerFactory factory) {
        sAppMenuHandlerFactory = factory;
    }

    private final Context mContext;
    private final MenuButtonDelegate mButtonDelegate;
    private final AppMenuDelegate mAppMenuDelegate;

    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuHandlerImpl mAppMenuHandler;

    private static AppMenuHandlerFactory sAppMenuHandlerFactory = (delegate, appMenuDelegate,
            menuResourceId, decorView, activityLifecycleDispatcher, overviewModeBehaviorSupplier)
            -> new AppMenuHandlerImpl(delegate, appMenuDelegate, menuResourceId, decorView,
                    activityLifecycleDispatcher, overviewModeBehaviorSupplier);

    /**
     * Construct a new AppMenuCoordinatorImpl.
     * @param context The activity context.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *         activity.
     * @param buttonDelegate The {@link ToolbarManager} for the containing activity.
     * @param appMenuDelegate The {@link AppMenuDelegate} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *         activity.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     */
    public AppMenuCoordinatorImpl(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            MenuButtonDelegate buttonDelegate, AppMenuDelegate appMenuDelegate, View decorView,
            @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        mContext = context;
        mButtonDelegate = buttonDelegate;
        mAppMenuDelegate = appMenuDelegate;
        mAppMenuPropertiesDelegate = mAppMenuDelegate.createAppMenuPropertiesDelegate();

        mAppMenuHandler = sAppMenuHandlerFactory.get(mAppMenuPropertiesDelegate, mAppMenuDelegate,
                mAppMenuPropertiesDelegate.getAppMenuLayoutId(), decorView,
                activityLifecycleDispatcher, overviewModeBehaviorSupplier);

        // TODO(twellington): Move to UpdateMenuItemHelper or common UI coordinator parent?
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible) return;

                mAppMenuPropertiesDelegate.onMenuDismissed();
                MenuItem updateMenuItem =
                        mAppMenuHandler.getAppMenu().getMenu().findItem(R.id.update_menu_id);
                if (updateMenuItem != null && updateMenuItem.isVisible()) {
                    UpdateMenuItemHelper.getInstance().onMenuDismissed();
                }
            }

            @Override
            public void onMenuHighlightChanged(boolean highlighting) {}
        });
    }

    @Override
    public void destroy() {
        // Prevent the menu window from leaking.
        if (mAppMenuHandler != null) mAppMenuHandler.destroy();

        mAppMenuPropertiesDelegate.destroy();
    }

    @Override
    public void showAppMenuForKeyboardEvent() {
        if (mAppMenuHandler == null || !mAppMenuHandler.shouldShowAppMenu()) return;

        boolean hasPermanentMenuKey = ViewConfiguration.get(mContext).hasPermanentMenuKey();
        mAppMenuHandler.showAppMenu(
                hasPermanentMenuKey ? null : mButtonDelegate.getMenuButtonView(), false,
                mButtonDelegate.isMenuFromBottom());
    }

    @Override
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    @Override
    public AppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return mAppMenuPropertiesDelegate;
    }

    @Override
    public void registerAppMenuBlocker(AppMenuBlocker blocker) {
        mAppMenuHandler.registerAppMenuBlocker(blocker);
    }

    @Override
    public void unregisterAppMenuBlocker(AppMenuBlocker blocker) {
        mAppMenuHandler.unregisterAppMenuBlocker(blocker);
    }

    // Testing methods

    @VisibleForTesting
    public AppMenuHandlerImpl getAppMenuHandlerImplForTesting() {
        return mAppMenuHandler;
    }
}

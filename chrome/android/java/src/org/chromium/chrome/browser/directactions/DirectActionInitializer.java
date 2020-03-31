// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Bundle;
import android.os.CancellationSignal;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.function.Consumer;

/**
 * A wrapper for initializing {@link DirectActionCoordinator} with standard direct actions from
 * Chrome activities.
 *
 * <p>To extend the set of direct actions beyond what's provided by this class, register handlers to
 * the coordinator {@code mCoordinator}.
 */
@TargetApi(29)
public class DirectActionInitializer implements NativeInitObserver, Destroyable {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ChromeFullscreenManager mFullscreenManager;
    private final CompositorViewHolder mCompositorViewHolder;
    private final ActivityTabProvider mActivityTabProvider;
    private final TabModelSelector mTabModelSelector;
    private final ScrimView mScrim;

    @ActivityType
    private int mActivityType;
    private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private Runnable mGoBackAction;
    @Nullable
    private FindToolbarManager mFindToolbarManager;
    private boolean mDirectActionsRegistered;
    @Nullable
    private DirectActionCoordinator mCoordinator;
    @Nullable
    private MenuDirectActionHandler mMenuHandler;

    /**
     * @param context The current context, often the activity instance.
     * @param activityType The type of the current activity
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param findToolbarManager Manager to use for the "find_in_page" action, if it exists
     * @param bottomSheetController Controller for the activity's bottom sheet, if it exists
     * @param fullscreenManager fullscreen manager of the activity
     * @param compositorViewHolder compositor view holder of the activity
     * @param activityTabProvider activity tab provider
     * @param scrim The activity's scrim view, if it exists
     */
    public DirectActionInitializer(Context context, @ActivityType int activityType,
            MenuOrKeyboardActionController actionController, Runnable goBackAction,
            TabModelSelector tabModelSelector, @Nullable FindToolbarManager findToolbarManager,
            @Nullable BottomSheetController bottomSheetController,
            ChromeFullscreenManager fullscreenManager, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider, ScrimView scrim) {
        mContext = context;
        mActivityType = activityType;
        mMenuOrKeyboardActionController = actionController;
        mGoBackAction = goBackAction;
        mTabModelSelector = tabModelSelector;
        mFindToolbarManager = findToolbarManager;
        mBottomSheetController = bottomSheetController;
        mFullscreenManager = fullscreenManager;
        mCompositorViewHolder = compositorViewHolder;
        mActivityTabProvider = activityTabProvider;
        mScrim = scrim;

        mDirectActionsRegistered = false;
    }

    /**
     * Performs a direct action.
     *
     * @param actionId Name of the direct action to perform.
     * @param arguments Arguments for this action.
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onPerformDirectAction(String actionId, Bundle arguments,
            CancellationSignal cancellationSignal, Consumer<Bundle> callback) {
        if (mCoordinator == null || !mDirectActionsRegistered) {
            callback.accept(Bundle.EMPTY);
            return;
        }
        mCoordinator.onPerformDirectAction(actionId, arguments, callback);
    }

    /**
     * Lists direct actions supported.
     *
     * Returns a list of direct actions supported by the Activity associated with this
     * RootUiCoordinator.
     *
     * @param cancellationSignal Signal used to cancel a direct action from the caller.
     * @param callback Callback to run when the action is done.
     */
    public void onGetDirectActions(CancellationSignal cancellationSignal, Consumer callback) {
        if (mCoordinator == null || !mDirectActionsRegistered) {
            callback.accept(Bundle.EMPTY);
            return;
        }
        mCoordinator.onGetDirectActions(callback);
    }

    /**
     * Registers common action that manipulate the current activity or the browser content.
     *
     * @param context The current context, often the activity instance.
     * @param activityType The type of the current activity
     * @param actionController Controller to use to execute menu actions
     * @param goBackAction Implementation of the "go_back" action, usually {@link
     *         android.app.Activity#onBackPressed}.
     * @param tabModelSelector The activity's {@link TabModelSelector}
     * @param findToolbarManager Manager to use for the "find_in_page" action, if it exists
     * @param bottomSheetController Controller for the activity's bottom sheet, if it exists
     * @param fullscreenManager fullscreen manager of the activity
     * @param compositorViewHolder compositor view holder of the activity
     * @param activityTabProvider activity tab provider
     * @param scrim The activity's scrim view, if it exists
     */
    private void registerCommonChromeActions(Context context, @ActivityType int activityType,
            MenuOrKeyboardActionController actionController, Runnable goBackAction,
            TabModelSelector tabModelSelector, @Nullable FindToolbarManager findToolbarManager,
            @Nullable BottomSheetController bottomSheetController,
            ChromeFullscreenManager fullscreenManager, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider, ScrimView scrim) {
        mCoordinator.register(new GoBackDirectActionHandler(goBackAction));
        mCoordinator.register(
                new FindInPageDirectActionHandler(tabModelSelector, findToolbarManager));

        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .whitelistActions(R.id.forward_menu_id, R.id.reload_menu_id);

        if (AutofillAssistantFacade.areDirectActionsAvailable(activityType)) {
            DirectActionHandler handler = AutofillAssistantFacade.createDirectActionHandler(context,
                    bottomSheetController, fullscreenManager, compositorViewHolder,
                    activityTabProvider, scrim);
            if (handler != null) mCoordinator.register(handler);
        }
    }

    /**
     * Registers actions that manipulate tabs in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    void registerTabManipulationActions(
            MenuOrKeyboardActionController actionController, TabModelSelector tabModelSelector) {
        registerMenuHandlerIfNecessary(actionController, tabModelSelector).allowAllActions();
        mCoordinator.register(new CloseTabDirectActionHandler(tabModelSelector));
    }

    /**
     * Allows a specific set of menu actions in addition to the common actions.
     *
     * @param actionController Controller to use to execute menu action
     * @param tabModelSelector The activity's {@link TabModelSelector}
     */
    void allowMenuActions(MenuOrKeyboardActionController actionController,
            TabModelSelector tabModelSelector, Integer... itemIds) {
        registerMenuHandlerIfNecessary(actionController, tabModelSelector)
                .whitelistActions(itemIds);
    }

    // Implements Destroyable
    @Override
    public void destroy() {
        mCoordinator = null;
        mDirectActionsRegistered = false;
    }

    // Implements NativeInitObserver
    @Override
    public void onFinishNativeInitialization() {
        mCoordinator = AppHooks.get().createDirectActionCoordinator();
        if (mCoordinator != null) {
            mCoordinator.init(/* isEnabled= */ () -> !mTabModelSelector.isIncognitoSelected());
            registerDirectActions();
        }
    }

    /**
     * Registers the set of direct actions available to assist apps.
     */
    void registerDirectActions() {
        registerCommonChromeActions(mContext, mActivityType, mMenuOrKeyboardActionController,
                mGoBackAction, mTabModelSelector, mFindToolbarManager,
                AutofillAssistantFacade.areDirectActionsAvailable(mActivityType)
                        ? mBottomSheetController
                        : null,
                mFullscreenManager, mCompositorViewHolder, mActivityTabProvider, mScrim);

        if (mActivityType == ActivityType.TABBED) {
            registerTabManipulationActions(mMenuOrKeyboardActionController, mTabModelSelector);
        } else if (mActivityType == ActivityType.CUSTOM_TAB) {
            allowMenuActions(mMenuOrKeyboardActionController, mTabModelSelector,
                    R.id.bookmark_this_page_id, R.id.preferences_id);
        }

        mDirectActionsRegistered = true;
    }

    private MenuDirectActionHandler registerMenuHandlerIfNecessary(
            MenuOrKeyboardActionController actionController, TabModelSelector tabModelSelector) {
        if (mMenuHandler == null) {
            mMenuHandler = new MenuDirectActionHandler(actionController, tabModelSelector);
            mCoordinator.register(mMenuHandler);
        }
        return mMenuHandler;
    }
}

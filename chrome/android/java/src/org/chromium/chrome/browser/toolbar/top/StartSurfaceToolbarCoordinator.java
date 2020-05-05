// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonCoordinator;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonView;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the StartSurfaceToolbar. This class handles all interactions that the
 * StartSurfaceToolbar has with the outside world. Lazily creates the tab toolbar the first time
 * it's needed.
 */
class StartSurfaceToolbarCoordinator {
    private final StartSurfaceToolbarMediator mToolbarMediator;
    private final ViewStub mStub;
    private final PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private StartSurfaceToolbarView mView;
    private TabModelSelector mTabModelSelector;
    private IncognitoSwitchCoordinator mIncognitoSwitchCoordinator;
    private TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;
    private TabSwitcherButtonView mTabSwitcherButtonView;
    private TabCountProvider mTabCountProvider;
    private ThemeColorProvider mThemeColorProvider;
    private OnClickListener mTabSwitcherClickListener;
    private OnLongClickListener mTabSwitcherLongClickListener;

    StartSurfaceToolbarCoordinator(ViewStub startSurfaceToolbarStub,
            IdentityDiscController identityDiscController,
            UserEducationHelper userEducationHelper) {
        mStub = startSurfaceToolbarStub;
        mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE,
                                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER
                                                .getValue()
                                        ? false
                                        : true)
                        .with(StartSurfaceToolbarProperties.IN_START_SURFACE_MODE, false)
                        .with(StartSurfaceToolbarProperties.MENU_IS_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IS_VISIBLE, true)
                        .build();

        // START_SURFACE_HIDE_INCOGNITO_SWITCH and START_SURFACE_SHOW_STACK_TAB_SWITCHER should not
        // be both true.
        assert !(StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH.getValue()
                && StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue());
        mToolbarMediator = new StartSurfaceToolbarMediator(mPropertyModel, identityDiscController,
                (iphCommandBuilder)
                        -> {
                    // TODO(crbug.com/865801): Replace the null check with an assert after fixing or
                    // removing the ShareButtonControllerTest that necessitated it.
                    if (mView == null) return;
                    userEducationHelper.requestShowIPH(
                            iphCommandBuilder.setAnchorView(mView.getIdentityDiscView()).build());
                },
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH.getValue(),
                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue());
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        mToolbarMediator.destroy();
        if (mIncognitoSwitchCoordinator != null) mIncognitoSwitchCoordinator.destroy();
        if (mTabSwitcherButtonCoordinator != null) mTabSwitcherButtonCoordinator.destroy();
        mTabSwitcherButtonCoordinator = null;
        mTabSwitcherButtonView = null;
        mTabCountProvider = null;
        mThemeColorProvider = null;
        mTabSwitcherClickListener = null;
        mTabSwitcherLongClickListener = null;
    }
    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarMediator.setAppMenuButtonHelper(appMenuButtonHelper);
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mToolbarMediator.setOnNewTabClickHandler(listener);
    }

    /**
     * Sets the current {@Link TabModelSelector} so the toolbar can pass it into buttons that need
     * access to it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        mToolbarMediator.setTabModelSelector(selector);
    }

    /**
     * Called when Start Surface mode is entered or exited.
     * @param inStartSurfaceMode Whether or not start surface mode should be shown or hidden.
     */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        if (!isInflated()) {
            inflate();
        }
        mToolbarMediator.setStartSurfaceMode(inStartSurfaceMode);
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mToolbarMediator.setIncognitoStateProvider(provider);
    }

    /**
     * Called to set the visibility in Start Surface mode.
     * @param shouldShowStartSurfaceToolbar whether the toolbar should be visible.
     */
    void setStartSurfaceToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mToolbarMediator.setStartSurfaceToolbarVisibility(shouldShowStartSurfaceToolbar);
    }

    /**
     * Called when accessibility status changes.
     * @param enabled whether accessibility status is enabled.
     */
    void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarMediator.onAccessibilityStatusChanged(enabled);
    }

    /**
     * @param isVisible Whether the bottom toolbar is visible.
     */
    void onBottomToolbarVisibilityChanged(boolean isVisible) {
        mToolbarMediator.onBottomToolbarVisibilityChanged(isVisible);
    }

    /**
     * @param overviewModeBehavior The {@link OverviewModeBehavior} to observe overview state
     *         changes.
     */
    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mToolbarMediator.setOverviewModeBehavior(overviewModeBehavior);
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} to update the tab switcher button.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.setTabCountProvider(tabCountProvider);
        } else {
            mTabCountProvider = tabCountProvider;
        }
    }

    /**
     * @param themeColorProvider The {@link ThemeColorProvider} to update the tab switcher button.
     */
    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.setThemeColorProvider(themeColorProvider);
        } else {
            mThemeColorProvider = themeColorProvider;
        }
    }

    /**
     * @param onClickListener The {@link OnClickListener} for the tab switcher button.
     */
    void setTabSwitcherListener(OnClickListener onClickListener) {
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.setTabSwitcherListener(onClickListener);
        } else {
            mTabSwitcherClickListener = onClickListener;
        }
    }

    /**
     * @param listener The {@link OnLongClickListener} for the tab switcher button.
     */
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {
        if (mTabSwitcherButtonView != null) {
            mTabSwitcherButtonView.setOnLongClickListener(listener);
        } else {
            mTabSwitcherLongClickListener = listener;
        }
    }

    void onNativeLibraryReady() {
        mToolbarMediator.onNativeLibraryReady();
    }

    private void inflate() {
        mStub.setLayoutResource(R.layout.start_top_toolbar);
        mView = (StartSurfaceToolbarView) mStub.inflate();
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mView, StartSurfaceToolbarViewBinder::bind);

        if (IncognitoUtils.isIncognitoModeEnabled()
                && !StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue()) {
            mIncognitoSwitchCoordinator = new IncognitoSwitchCoordinator(mView, mTabModelSelector);
        }

        if (StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue()) {
            mTabSwitcherButtonView = mView.findViewById(R.id.start_tab_switcher_button);
            if (mTabSwitcherLongClickListener != null) {
                mTabSwitcherButtonView.setOnLongClickListener(mTabSwitcherLongClickListener);
                mTabSwitcherLongClickListener = null;
            }
            mTabSwitcherButtonCoordinator =
                    new TabSwitcherButtonCoordinator(mTabSwitcherButtonView);
            mTabSwitcherButtonView.setVisibility(View.VISIBLE);
            if (mTabCountProvider != null) {
                mTabSwitcherButtonCoordinator.setTabCountProvider(mTabCountProvider);
                mTabCountProvider = null;
            }
            if (mThemeColorProvider != null) {
                mTabSwitcherButtonCoordinator.setThemeColorProvider(mThemeColorProvider);
                mThemeColorProvider = null;
            }
            if (mTabSwitcherClickListener != null) {
                mTabSwitcherButtonCoordinator.setTabSwitcherListener(mTabSwitcherClickListener);
                mTabSwitcherClickListener = null;
            }
        }
    }

    private boolean isInflated() {
        return mView != null;
    }
}

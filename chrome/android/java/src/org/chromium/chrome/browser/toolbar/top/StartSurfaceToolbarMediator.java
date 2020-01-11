// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.APP_MENU_BUTTON_HELPER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;

import android.view.View;

import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements interacts between the views and the caller. */
class StartSurfaceToolbarMediator {
    private final PropertyModel mPropertyModel;
    private TabModelSelector mTabModelSelector;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private OverviewModeBehavior mOverviewModeBehavior;
    private OverviewModeObserver mOverviewModeObserver;
    @OverviewModeState
    private int mOverviewModeState;
    private boolean mIsGoogleSearchEngine;

    StartSurfaceToolbarMediator(PropertyModel model) {
        mPropertyModel = model;
        mPropertyModel.set(MENU_IS_VISIBLE, !FeatureUtilities.isBottomToolbarEnabled());
        mOverviewModeState = OverviewModeState.NOT_SHOWN;
    }

    void onNativeLibraryReady() {
        assert mTemplateUrlObserver == null;

        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            @Override
            public void onTemplateURLServiceChanged() {
                updateLogoVisibility(mOverviewModeState,
                        TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
            }
        };

        TemplateUrlServiceFactory.get().addObserver(mTemplateUrlObserver);
        mIsGoogleSearchEngine = TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
        updateLogoVisibility(
                mOverviewModeState, TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
    }

    void destroy() {
        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
        }
        if (mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        if (mOverviewModeObserver != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
    }

    void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mPropertyModel.set(APP_MENU_BUTTON_HELPER, appMenuButtonHelper);
    }

    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mPropertyModel.set(NEW_TAB_CLICK_HANDLER, listener);
    }

    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;

        if (mTabModelSelectorObserver == null) {
            mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
                }
            };
        }
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mPropertyModel.set(IS_VISIBLE, inStartSurfaceMode);
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mPropertyModel.set(INCOGNITO_STATE_PROVIDER, provider);
    }

    void setStartSurfaceToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mPropertyModel.set(IS_VISIBLE, shouldShowStartSurfaceToolbar);
    }

    void onAccessibilityStatusChanged(boolean enabled) {
        mPropertyModel.set(ACCESSIBILITY_ENABLED, enabled);
    }

    void onBottomToolbarVisibilityChanged(boolean isVisible) {
        mPropertyModel.set(MENU_IS_VISIBLE, !isVisible);
    }

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert mOverviewModeBehavior == null;

        mOverviewModeBehavior = overviewModeBehavior;
        if (mOverviewModeObserver == null) {
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStateChanged(
                        @OverviewModeState int overviewModeState, boolean showTabSwitcherToolbar) {
                    boolean isShownTabswitcherState =
                            overviewModeState == OverviewModeState.SHOWN_TABSWITCHER;
                    mPropertyModel.set(NEW_TAB_BUTTON_IS_VISIBLE, isShownTabswitcherState);
                    updateLogoVisibility(overviewModeState, mIsGoogleSearchEngine);
                }
                @Override
                public void onOverviewModeFinishedShowing() {
                    mPropertyModel.set(BUTTONS_CLICKABLE, true);
                }
                @Override
                public void onOverviewModeStartedHiding(
                        boolean showToolbar, boolean delayAnimation) {
                    mPropertyModel.set(BUTTONS_CLICKABLE, false);
                }

            };
        }
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    private void updateLogoVisibility(
            @OverviewModeState int overviewModeState, boolean isGoogleSearchEngine) {
        mOverviewModeState = overviewModeState;
        mIsGoogleSearchEngine = isGoogleSearchEngine;
        boolean shouldShowLogo =
                (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE
                        || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY)
                && mIsGoogleSearchEngine;
        mPropertyModel.set(LOGO_IS_VISIBLE, shouldShowLogo);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.APP_MENU_BUTTON_HELPER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IPH;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_AT_LEFT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IPHContainer;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.util.AccessibilityUtil;
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
    private boolean mEnableIdentityDisc;

    StartSurfaceToolbarMediator(PropertyModel model) {
        mPropertyModel = model;
        mPropertyModel.set(MENU_IS_VISIBLE, true);
        mOverviewModeState = OverviewModeState.NOT_SHOWN;
    }

    void onNativeLibraryReady() {
        assert mTemplateUrlObserver == null;

        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            @Override
            public void onTemplateURLServiceChanged() {
                updateLogoVisibility(TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
            }
        };

        TemplateUrlServiceFactory.get().addObserver(mTemplateUrlObserver);
        mIsGoogleSearchEngine = TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
        updateLogoVisibility(TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
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
                    updateIdentityDiscVisibility();
                    if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY) {
                        mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, hasIncognitoTabs());
                    }
                }
            };
        }
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        updateIdentityDiscVisibility();
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    // TODO(crbug.com/1042997): share with TabSwitcherModeTTPhone.
    private boolean hasIncognitoTabs() {
        // Check if there is no incognito tab, or all the incognito tabs are being closed.
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        for (int i = 0; i < incognitoTabModel.getCount(); i++) {
            if (!incognitoTabModel.getTabAt(i).isClosing()) return true;
        }
        assert !mTabModelSelector.isIncognitoSelected();
        return false;
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
        updateNewTabButtonVisibility();
    }

    void onBottomToolbarVisibilityChanged(boolean isVisible) {}

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert mOverviewModeBehavior == null;

        mOverviewModeBehavior = overviewModeBehavior;
        if (mOverviewModeObserver == null) {
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStateChanged(
                        @OverviewModeState int overviewModeState, boolean showTabSwitcherToolbar) {
                    mOverviewModeState = overviewModeState;
                    updateNewTabButtonVisibility();
                    updateLogoVisibility(mIsGoogleSearchEngine);
                    updateIdentityDiscVisibility();
                }
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    if (mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY) {
                        mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, hasIncognitoTabs());
                        mPropertyModel.set(NEW_TAB_BUTTON_AT_LEFT, true);
                    }
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

    private void updateLogoVisibility(boolean isGoogleSearchEngine) {
        mIsGoogleSearchEngine = isGoogleSearchEngine;
        boolean shouldShowLogo =
                (mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE
                        || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY
                        || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY)
                && mIsGoogleSearchEngine;
        mPropertyModel.set(LOGO_IS_VISIBLE, shouldShowLogo);
    }

    void enableIdentityDisc(View.OnClickListener onClickListener, Drawable image,
            @StringRes int contentDescriptionResId) {
        mEnableIdentityDisc = true;
        mPropertyModel.set(IDENTITY_DISC_CLICK_HANDLER, onClickListener);
        mPropertyModel.set(IDENTITY_DISC_IMAGE, image);
        mPropertyModel.set(IDENTITY_DISC_DESCRIPTION, contentDescriptionResId);
        updateIdentityDiscVisibility();
    }

    void updateIdentityDiscImage(Drawable image) {
        mPropertyModel.set(IDENTITY_DISC_IMAGE, image);
    }

    void disableIdentityDisc() {
        mEnableIdentityDisc = false;
        mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, false);
    }

    void showIPHOnIdentityDisc(@StringRes int stringId, @StringRes int accessibilityStringId,
            Runnable dismissedCallback) {
        // Only show IPH if IdentityDisc is actually visible, otherwise dismiss.
        if (mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE)) {
            mPropertyModel.set(IDENTITY_DISC_IPH,
                    new IPHContainer(stringId, accessibilityStringId, dismissedCallback));
        } else if (dismissedCallback != null) {
            dismissedCallback.run();
        }
    }

    private void updateNewTabButtonVisibility() {
        // This toolbar is only shown for tab switcher when accessibility is enabled. Note that
        // OverviewListLayout will be shown as the tab switcher instead of the star surface.
        boolean isShownTabswitcherState = mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER
                || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY
                || mOverviewModeState == OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                || AccessibilityUtil.isAccessibilityEnabled();
        mPropertyModel.set(NEW_TAB_BUTTON_IS_VISIBLE, isShownTabswitcherState);
    }

    private void updateIdentityDiscVisibility() {
        mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE,
                mOverviewModeState == OverviewModeState.SHOWN_HOMEPAGE && mEnableIdentityDisc
                        && !mTabModelSelector.isIncognitoSelected());
    }
}

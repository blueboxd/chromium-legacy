// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.ColorStateList;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the tab switcher button. This class handles all interactions that the tab
 * switcher button has with the outside world.
 */
public class TabSwitcherButtonCoordinator {
    /**
     *  The model that handles events from outside the tab switcher button. Normally the coordinator
     *  should acces the mediator which then updates the model. Since this component is very simple
     *  the mediator is omitted.
     */
    private final PropertyModel mTabSwitcherButtonModel =
            new PropertyModel(TabSwitcherButtonProperties.ALL_KEYS);

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;

    private ThemeColorProvider mThemeColorProvider;
    private TintObserver mTintObserver;

    private TabCountProvider mTabCountProvider;
    private TabCountObserver mTabCountObserver;

    /** The {@link OverviewModeBehavior} used to observe overview state changes.  */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** The {@link OvervieModeObserver} observing the OverviewModeBehavior  */
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    @OverviewModeState
    private int mOverviewModeState;

    /**
     * Build the controller that manages the tab switcher button.
     * @param view The {@link TabSwitcherButtonView} the controller manages.
     */
    public TabSwitcherButtonCoordinator(TabSwitcherButtonView view) {
        PropertyModelChangeProcessor.create(
                mTabSwitcherButtonModel, view, new TabSwitcherButtonViewBinder());
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStateChanged(
                    @OverviewModeState int overviewModeState, boolean showTabSwitcherToolbar) {
                mOverviewModeState = overviewModeState;
                updateButtonState();
            }
        };

        mOverviewModeState = OverviewModeState.NOT_SHOWN;
    }

    /**
     * @param onClickListener An {@link OnClickListener} that is triggered when the tab switcher
     *                        button is clicked.
     */
    public void setTabSwitcherListener(OnClickListener onClickListener) {
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.ON_CLICK_LISTENER, onClickListener);
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mTintObserver = new TintObserver() {
            @Override
            public void onTintChanged(ColorStateList tint, boolean useLight) {
                mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.TINT, tint);
            }
        };
        mThemeColorProvider.addTintObserver(mTintObserver);
    }

    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountObserver = new TabCountObserver() {
            @Override
            public void onTabCountChanged(int tabCount, boolean isIncognito) {
                mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.NUMBER_OF_TABS, tabCount);
                updateButtonState();
            }
        };
        mTabCountProvider.addObserverAndTrigger(mTabCountObserver);
    }

    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert overviewModeBehavior != null;
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(mTintObserver);
            mThemeColorProvider = null;
        }
        if (mTabCountProvider != null) {
            mTabCountProvider.removeObserver(mTabCountObserver);
            mTabCountProvider = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeObserver = null;
        }
    }

    private void updateButtonState() {
        // TODO(crbug.com/1039997): match SHOWN_HOMEPAGE instead.
        boolean shouldEnable = mOverviewModeState != OverviewModeState.SHOWN_TABSWITCHER
                && mOverviewModeState != OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY
                && mOverviewModeState != OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                && mOverviewModeState != OverviewModeState.SHOWN_TABSWITCHER_TWO_PANES
                && mTabSwitcherButtonModel.get(TabSwitcherButtonProperties.NUMBER_OF_TABS) >= 1;
        mTabSwitcherButtonModel.set(TabSwitcherButtonProperties.IS_ENABLED, shouldEnable);
    }
}

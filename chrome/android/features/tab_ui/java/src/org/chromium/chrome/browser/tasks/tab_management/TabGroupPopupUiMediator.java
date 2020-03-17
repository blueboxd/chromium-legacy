// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGroupPopUi. Responsible for managing the
 * internal state of the component.
 */
public class TabGroupPopupUiMediator {
    /**
     * An interface to update the size of the TabGroupPopupUi.
     */
    public interface TabGroupPopUiUpdater {
        /**
         * Update the TabGroupPopUi.
         */
        void updateTabGroupPopUi();
    }

    private final PropertyModel mModel;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelector mTabModelSelector;
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private final ChromeFullscreenManager mFullscreenManager;
    private final ChromeFullscreenManager.FullscreenListener mFullscreenListener;
    private final KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private final TabGroupPopUiUpdater mUiUpdater;
    private final TabGroupUiMediator.TabGroupUiController mTabGroupUiController;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private boolean mIsOverviewModeVisible;

    TabGroupPopupUiMediator(PropertyModel model, TabModelSelector tabModelSelector,
            OverviewModeBehavior overviewModeBehavior, ChromeFullscreenManager fullscreenManager,
            TabGroupPopUiUpdater updater, TabGroupUiMediator.TabGroupUiController controller,
            BottomSheetController bottomSheetController) {
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mOverviewModeBehavior = overviewModeBehavior;
        mFullscreenManager = fullscreenManager;
        mUiUpdater = updater;
        mTabGroupUiController = controller;
        mBottomSheetController = bottomSheetController;

        mFullscreenListener = new ChromeFullscreenManager.FullscreenListener() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                // Modify the alpha the strip container view base on bottomOffset. The range of
                // bottomOffset is within 0 to mIconSize.
                mModel.set(TabGroupPopupUiProperties.CONTENT_VIEW_ALPHA,
                        1 - mFullscreenManager.getBrowserControlHiddenRatio());
            }
        };
        mFullscreenManager.addListener(mFullscreenListener);

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                List<Tab> tabList = mTabModelSelector.getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()
                                            .getRelatedTabList(lastId);
                if (tabList.contains(tab)) return;
                if (isCurrentTabInGroup()) {
                    if (isTabStripShowing()) {
                        mUiUpdater.updateTabGroupPopUi();
                    } else {
                        maybeShowTabStrip();
                    }
                } else {
                    hideTabStrip();
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (!isCurrentTabInGroup()) {
                    hideTabStrip();
                }
                if (isTabStripShowing()) {
                    mUiUpdater.updateTabGroupPopUi();
                }
            }

            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState) {
                if (isTabStripShowing()) {
                    mUiUpdater.updateTabGroupPopUi();
                    return;
                }
                if (type != TabLaunchType.FROM_RESTORE) {
                    maybeShowTabStrip();
                }
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (isTabStripShowing()) {
                    mUiUpdater.updateTabGroupPopUi();
                    return;
                }
                maybeShowTabStrip();
            }

            @Override
            public void restoreCompleted() {
                maybeShowTabStrip();
            }
        };
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mIsOverviewModeVisible = true;
                hideTabStrip();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mIsOverviewModeVisible = false;
                maybeShowTabStrip();
            }
        };
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);

        mKeyboardVisibilityListener = new KeyboardVisibilityDelegate.KeyboardVisibilityListener() {
            private boolean mWasShowingStrip;
            @Override
            public void keyboardVisibilityChanged(boolean isShowing) {
                if (isShowing) {
                    mWasShowingStrip = isTabStripShowing();
                    hideTabStrip();
                } else {
                    if (mWasShowingStrip) {
                        maybeShowTabStrip();
                    }
                }
            }
        };
        KeyboardVisibilityDelegate.getInstance().addKeyboardVisibilityListener(
                mKeyboardVisibilityListener);

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            private Boolean mWasShowingStrip;
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheetController.SheetState.HIDDEN) {
                    if (mWasShowingStrip != null && mWasShowingStrip) {
                        maybeShowTabStrip();
                    }
                    mWasShowingStrip = null;
                } else {
                    if (mWasShowingStrip == null) {
                        mWasShowingStrip = isTabStripShowing();
                    }
                    hideTabStrip();
                }
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);

        // TODO(yuezhanggg): Reset the strip with empty tab list as well.
        mTabGroupUiController.setupLeftButtonOnClickListener(view -> hideTabStrip());
    }

    void onAnchorViewChanged(View anchorView, int anchorViewId) {
        boolean isShowing = isTabStripShowing();
        if (isShowing) {
            mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
        }
        // When showing bottom toolbar, the arrow on dismiss button should point down; when showing
        // adaptive toolbar, the arrow on dismiss button should point up.
        mTabGroupUiController.setupLeftButtonDrawable(anchorViewId == R.id.toolbar
                        ? R.drawable.ic_expand_less_black_24dp
                        : R.drawable.ic_expand_more_black_24dp);
        mModel.set(TabGroupPopupUiProperties.ANCHOR_VIEW, anchorView);
        if (isShowing) {
            mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
        }
    }

    void maybeShowTabStrip() {
        if (mIsOverviewModeVisible || !isCurrentTabInGroup()) return;
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, true);
    }

    private void hideTabStrip() {
        mModel.set(TabGroupPopupUiProperties.IS_VISIBLE, false);
    }

    private boolean isCurrentTabInGroup() {
        assert mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return false;
        List<Tab> tabgroup = filter.getRelatedTabList(currentTab.getId());
        return tabgroup.size() > 1;
    }

    private boolean isTabStripShowing() {
        return mModel.get(TabGroupPopupUiProperties.IS_VISIBLE);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        KeyboardVisibilityDelegate.getInstance().removeKeyboardVisibilityListener(
                mKeyboardVisibilityListener);
        mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
        mFullscreenManager.removeListener(mFullscreenListener);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @VisibleForTesting
    boolean getIsOverviewModeVisibleForTesting() {
        return mIsOverviewModeVisible;
    }
}

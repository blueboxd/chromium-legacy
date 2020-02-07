// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A coordinator for TabStrip component as a popup window in bottom toolbar.
 */
public class TabGroupPopupUiCoordinator
        implements TabGroupPopupUi, Destroyable, TabGroupPopupUiMediator.TabGroupPopUiUpdater {
    private final ThemeColorProvider mThemeColorProvider;
    private final ObservableSupplier<View> mAnchorViewSupplier;
    private final Callback<View> mAnchorViewSupplierCallback;

    private PropertyModelChangeProcessor mModelChangeProcessor;
    private View mAnchorView;
    private TabGroupUiCoordinator mTabGroupUiCoordinator;
    private TabGroupPopupUiMediator mMediator;
    private TabGroupPopupUiParent mTabGroupPopupUiParent;

    TabGroupPopupUiCoordinator(
            ThemeColorProvider themeColorProvider, ObservableSupplier<View> parentViewSupplier) {
        mThemeColorProvider = themeColorProvider;
        mAnchorViewSupplier = parentViewSupplier;
        mAnchorViewSupplierCallback = this::onAnchorViewChanged;
        mAnchorViewSupplier.addObserver(mAnchorViewSupplierCallback);
    }

    // TabGroupPopUi implementation.
    @Override
    // TODO(crbug.com/1022827): Narrow down the dependencies required here and in
    // TabGroupUiCoordinator instead of passing in ChromeActivity.
    public void initializeWithNative(ChromeActivity activity) {
        PropertyModel model = new PropertyModel(TabGroupPopupUiProperties.ALL_KEYS);
        mTabGroupPopupUiParent = new TabGroupPopupUiParent(activity, mAnchorView);
        mTabGroupUiCoordinator = new TabGroupUiCoordinator(
                mTabGroupPopupUiParent.getCurrentContainerView(), mThemeColorProvider);
        mTabGroupUiCoordinator.initializeWithNative(activity, null);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, mTabGroupPopupUiParent, TabGroupPopupUiViewBinder::bind);
        mMediator = new TabGroupPopupUiMediator(model, activity.getTabModelSelector(),
                activity.getOverviewModeBehavior(), activity.getFullscreenManager(), this,
                mTabGroupUiCoordinator, activity.getBottomSheetController());
        mMediator.onAnchorViewChanged(mAnchorView, mAnchorView.getId());
    }

    @Override
    public View.OnLongClickListener getLongClickListenerForTriggering() {
        return v -> {
            mMediator.maybeShowTabStrip();
            return true;
        };
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
        }
        if (mTabGroupUiCoordinator != null) {
            mTabGroupUiCoordinator.destroy();
        }
        mAnchorViewSupplier.removeObserver(mAnchorViewSupplierCallback);
    }

    // TabGroupPopUiUpdater implementation.
    @Override
    public void updateTabGroupPopUi() {
        mTabGroupPopupUiParent.updateStripWindow();
    }

    private void onAnchorViewChanged(View anchorView) {
        if (mAnchorView == anchorView) return;
        mAnchorView = anchorView;
        if (mMediator == null) return;
        mMediator.onAnchorViewChanged(anchorView, anchorView.getId());
    }
}

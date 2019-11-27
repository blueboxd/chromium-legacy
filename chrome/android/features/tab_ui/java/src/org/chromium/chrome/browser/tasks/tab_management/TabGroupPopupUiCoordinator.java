// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.lifecycle.Destroyable;

/**
 * A coordinator for TabStrip component as a popup window in bottom toolbar.
 */
public class TabGroupPopupUiCoordinator implements TabGroupPopupUi, Destroyable {
    private final ThemeColorProvider mThemeColorProvider;
    private final ObservableSupplier<View> mAnchorViewSupplier;
    private final Callback<View> mAnchorViewSupplierCallback;

    private ChromeActivity mActivity;

    TabGroupPopupUiCoordinator(
            ThemeColorProvider themeColorProvider, ObservableSupplier<View> parentViewSupplier) {
        mThemeColorProvider = themeColorProvider;
        mAnchorViewSupplier = parentViewSupplier;
        mAnchorViewSupplierCallback = this::onAnchorViewChanged;
        mAnchorViewSupplier.addObserver(mAnchorViewSupplierCallback);
    }

    @Override
    // TODO(crbug.com/1022827): Narrow down the dependencies required here and in
    // TabGroupUiCoordinator instead of passing in ChromeActivity.
    public void initializeWithNative(ChromeActivity activity) {
        mActivity = activity;
    }

    private void onAnchorViewChanged(View v) {}

    @Override
    public View.OnLongClickListener getLongClickListenerForTriggering() {
        return v -> true;
    }

    @Override
    public void destroy() {
        mAnchorViewSupplier.removeObserver(mAnchorViewSupplierCallback);
    }
}

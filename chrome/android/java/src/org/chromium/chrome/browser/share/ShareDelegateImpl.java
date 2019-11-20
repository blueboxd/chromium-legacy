// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Implementation of share interface. Mostly a wrapper around ShareSheetCoordinator.
 */
public class ShareDelegateImpl implements ShareDelegate {
    private BottomSheetController mBottomSheetController;

    /**
     * Construct a new {@link ShareDelegateImpl}.
     * @param controller The BottomSheetController for the current activity.
     */
    public ShareDelegateImpl(BottomSheetController controller) {
        mBottomSheetController = controller;
    }

    // ShareDelegate implementation.
    @Override
    public void share(ShareParams params) {
        ShareSheetCoordinator coordinator = new ShareSheetCoordinator(mBottomSheetController);
        coordinator.share(params);
    }

    // ShareDelegate implementation.
    @Override
    public void share(Tab currentTab, boolean shareDirectly) {
        ShareSheetCoordinator coordinator = new ShareSheetCoordinator(mBottomSheetController);
        coordinator.onShareSelected(currentTab.getWindowAndroid().getActivity().get(), currentTab,
                shareDirectly, currentTab.isIncognito());
    }
}

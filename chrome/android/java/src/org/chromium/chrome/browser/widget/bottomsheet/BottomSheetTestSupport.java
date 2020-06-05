// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;

/** Utilities to support testing with the {@link BottomSheetController}. */
public class BottomSheetTestSupport {
    /** A handle to the actual implementation class of the {@link BottomSheetController}. */
    BottomSheetControllerImpl mController;

    /**
     * @param controller A handle to the public {@link BottomSheetController}.
     */
    public BottomSheetTestSupport(BottomSheetController controller) {
        mController = (BottomSheetControllerImpl) controller;
    }

    /** End all animations on the sheet for testing purposes. */
    public void endAllAnimations() {
        mController.endAnimationsForTesting();
    }

    /**
     * Force the sheet's state for testing.
     * @param state The state the sheet should be in.
     * @param animate Whether the sheet should animate to the specified state.
     */
    public void setSheetState(@SheetState int state, boolean animate) {
        mController.setSheetStateForTesting(state, animate);
    }
}

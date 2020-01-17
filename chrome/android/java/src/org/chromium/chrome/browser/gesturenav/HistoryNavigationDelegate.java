// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Provides navigation-related configuration.
 */
public interface HistoryNavigationDelegate {
    /**
     * @return {@link NavigationHandler#ActionDelegate} object.
     */
    NavigationHandler.ActionDelegate createActionDelegate();

    /**
     * @return {@link NavigationSheet#Delegate} object.
     */
    NavigationSheet.Delegate createSheetDelegate();

    /**
     * @return {@link BottomSheetController} object.
     */
    Supplier<BottomSheetController> getBottomSheetController();

    /**
     * Default {@link HistoryNavigationDelegate} that does not support navigation.
     */
    public static final HistoryNavigationDelegate DEFAULT = new HistoryNavigationDelegate() {
        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return null;
        }

        @Override
        public NavigationSheet.Delegate createSheetDelegate() {
            return null;
        }

        @Override
        public Supplier<BottomSheetController> getBottomSheetController() {
            assert false : "Should never be called";
            return null;
        }
    };
}

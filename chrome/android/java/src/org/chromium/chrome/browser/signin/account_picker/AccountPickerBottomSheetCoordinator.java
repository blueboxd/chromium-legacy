// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the account picker bottom sheet used in web signin flow.
 */
public class AccountPickerBottomSheetCoordinator {
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int newState) {
            super.onSheetStateChanged(newState);
            if (newState == BottomSheetController.SheetState.HIDDEN) {
                AccountPickerBottomSheetCoordinator.this.destroy();
            }
        }
    };

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottom sheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(Context context,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate) {
        AccountPickerBottomSheetView view = new AccountPickerBottomSheetView(context);
        mAccountPickerBottomSheetMediator =
                new AccountPickerBottomSheetMediator(context, accountPickerDelegate);
        mAccountPickerCoordinator = new AccountPickerCoordinator(
                view.getAccountListView(), mAccountPickerBottomSheetMediator, null);
        mBottomSheetController = bottomSheetController;
        PropertyModelChangeProcessor.create(mAccountPickerBottomSheetMediator.getModel(), view,
                AccountPickerBottomSheetViewBinder::bind);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(view, true);
    }

    /**
     * Releases the resources used by AccountPickerBottomSheetCoordinator.
     */
    @MainThread
    private void destroy() {
        mAccountPickerCoordinator.destroy();
        mAccountPickerBottomSheetMediator.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }
}

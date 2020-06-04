// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the account picker bottom sheet used in web signin flow.
 */
public class AccountPickerBottomSheetCoordinator {
    private final AccountPickerCoordinator mAccountPickerCoordinator;

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottomsheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(Context context,
            BottomSheetController bottomSheetController,
            AccountPickerCoordinator.Listener accountPickerListener) {
        AccountPickerBottomSheetView view = new AccountPickerBottomSheetView(context);
        mAccountPickerCoordinator = new AccountPickerCoordinator(
                view.getAccountPickerItemView(), accountPickerListener, null);
        PropertyModel model = AccountPickerBottomSheetProperties.createModel();
        PropertyModelChangeProcessor.create(model, view, AccountPickerBottomSheetViewBinder::bind);
        bottomSheetController.requestShowContent(view, true);
    }

    /**
     * Releases the resources used by AccountPickerBottomSheetCoordinator.
     */
    @MainThread
    public void destroy() {
        mAccountPickerCoordinator.destroy();
    }
}

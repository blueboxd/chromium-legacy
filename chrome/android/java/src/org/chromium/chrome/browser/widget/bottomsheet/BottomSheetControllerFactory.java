// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.view.View;
import android.view.Window;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** A factory for producing a {@link BottomSheetController}. */
public class BottomSheetControllerFactory {
    /**
     * @param scrim A supplier of scrim to be shown behind the sheet.
     * @param bottomSheetViewSupplier A means of creating the sheet's view.
     * @param window The activity's window.
     * @param keyboardDelegate A means of hiding the keyboard.
     * @return A new instance of the {@link BottomSheetController}.
     */
    public static BottomSheetControllerInternal createBottomSheetController(
            final Supplier<ScrimCoordinator> scrim, Supplier<View> bottomSheetViewSupplier,
            Window window, KeyboardVisibilityDelegate keyboardDelegate) {
        return new BottomSheetControllerImpl(
                scrim, bottomSheetViewSupplier, window, keyboardDelegate);
    }
}

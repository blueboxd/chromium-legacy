// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** A factory for producing a {@link BottomSheetController}. */
public class BottomSheetControllerFactory {
    /**
     * @param scrim A supplier of scrim to be shown behind the sheet.
     * @param initializedCallback A callback for the sheet having been created.
     * @param window The activity's window.
     * @param keyboardDelegate A means of hiding the keyboard.
     * @param root The view that should contain the sheet.
     * @param inflater A mechanism for building views from XML.
     * @return A new instance of the {@link BottomSheetController}.
     */
    public static ManagedBottomSheetController createBottomSheetController(
            final Supplier<ScrimCoordinator> scrim, Callback<View> initializedCallback,
            Window window, KeyboardVisibilityDelegate keyboardDelegate, Supplier<ViewGroup> root) {
        return new BottomSheetControllerImpl(
                scrim, initializedCallback, window, keyboardDelegate, root);
    }
}

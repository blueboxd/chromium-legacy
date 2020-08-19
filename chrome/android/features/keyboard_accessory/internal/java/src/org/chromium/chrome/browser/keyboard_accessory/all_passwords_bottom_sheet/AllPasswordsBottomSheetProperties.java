// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties defined here reflect the visible state of the AllPasswordsBottomSheet.
 */
class AllPasswordsBottomSheetProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");
    static final PropertyModel.ReadableObjectPropertyKey<ListModel<ListItem>> SHEET_ITEMS =
            new PropertyModel.ReadableObjectPropertyKey<>("sheet_items");

    static PropertyModel createDefaultModel(Callback<Integer> handler) {
        return new PropertyModel.Builder(VISIBLE, SHEET_ITEMS, DISMISS_HANDLER)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ListModel<>())
                .with(DISMISS_HANDLER, handler)
                .build();
    }
}

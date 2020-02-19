// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.content.Context;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.input.PopupItemType;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupItem;

import java.util.ArrayList;
import java.util.List;

/** JNI bridge between {@code interaction_handler_android} and Java. */
@JNINamespace("autofill_assistant")
public class AssistantViewInteractions {
    @CalledByNative
    private static void setOnClickListener(
            View view, String identifier, AssistantGenericUiDelegate delegate) {
        view.setOnClickListener(unused -> delegate.onViewClicked(identifier));
    }

    @CalledByNative
    private static void showListPopup(Context context, String[] itemNames,
            @PopupItemType int[] itemTypes, int[] selectedItems, boolean multiple,
            String identifier, AssistantGenericUiDelegate delegate) {
        assert (itemNames.length == itemTypes.length);
        List<SelectPopupItem> popupItems = new ArrayList<>();
        for (int i = 0; i < itemNames.length; i++) {
            popupItems.add(new SelectPopupItem(itemNames[i], itemTypes[i]));
        }

        SelectPopupDialog dialog = new SelectPopupDialog(context,
                (indices)
                        -> delegate.onListPopupSelectionChanged(
                                identifier, new AssistantValue(indices)),
                popupItems, multiple, selectedItems);
        dialog.show();
    }
}

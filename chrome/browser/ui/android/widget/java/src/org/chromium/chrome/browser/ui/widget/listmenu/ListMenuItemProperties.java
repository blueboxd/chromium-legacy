// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.listmenu;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/**
 * The properties controlling the state of the list menu items.
 */
public class ListMenuItemProperties {
    public static final WritableIntPropertyKey TITLE_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey START_ICON_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey END_ICON_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey TINT_COLOR_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
            TITLE_ID, START_ICON_ID, END_ICON_ID, MENU_ITEM_ID, ENABLED, TINT_COLOR_ID};
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;

import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the start surface toolbar properties. */
class StartSurfaceToolbarProperties {
    private StartSurfaceToolbarProperties() {}

    public static final PropertyModel
            .WritableObjectPropertyKey<AppMenuButtonHelper> APP_MENU_BUTTON_HELPER =
            new PropertyModel.WritableObjectPropertyKey<AppMenuButtonHelper>();
    public static final PropertyModel
            .WritableObjectPropertyKey<IncognitoStateProvider> INCOGNITO_STATE_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<IncognitoStateProvider>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> NEW_TAB_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey LOGO_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey ACCESSIBILITY_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey MENU_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_BUTTON_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {APP_MENU_BUTTON_HELPER, NEW_TAB_CLICK_HANDLER, IS_VISIBLE,
                    LOGO_IS_VISIBLE, IS_INCOGNITO, INCOGNITO_STATE_PROVIDER, ACCESSIBILITY_ENABLED,
                    MENU_IS_VISIBLE, NEW_TAB_BUTTON_IS_VISIBLE};
}

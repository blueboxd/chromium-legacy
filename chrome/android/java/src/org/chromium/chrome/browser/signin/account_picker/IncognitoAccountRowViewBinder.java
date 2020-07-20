// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.IncognitoAccountRowProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the buildView and bindView util methods of the
 * incognito account row.
 */
class IncognitoAccountRowViewBinder {
    private IncognitoAccountRowViewBinder() {}

    static View buildView(ViewGroup parent) {
        @LayoutRes
        int layoutRes = R.layout.account_picker_incognito_row;
        return LayoutInflater.from(parent.getContext()).inflate(layoutRes, parent, false);
    }

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IncognitoAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(
                    v -> model.get(IncognitoAccountRowProperties.ON_CLICK_LISTENER).run());
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }
}

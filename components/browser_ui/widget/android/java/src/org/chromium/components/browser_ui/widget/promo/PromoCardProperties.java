// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for PromoCardView.
 */
public class PromoCardProperties {
    // Visible related properties
    public static final WritableBooleanPropertyKey HAS_SECONDARY_BUTTON =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey HAS_CLOSE_BUTTON =
            new WritableBooleanPropertyKey();

    // View related properties
    public static final WritableObjectPropertyKey<Drawable> IMAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> SECONDARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ColorStateList> ICON_TINT =
            new WritableObjectPropertyKey<>();

    // Callback related properties
    public static final WritableObjectPropertyKey<Callback<View>> PRIMARY_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Callback<View>> SECONDARY_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Callback<View>> CLOSE_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    /**
     * All the property keys needed to create the model for {@link PromoCardView}.
     */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {HAS_SECONDARY_BUTTON, HAS_CLOSE_BUTTON, IMAGE, ICON_TINT, TITLE,
                    DESCRIPTION, PRIMARY_BUTTON_TEXT, SECONDARY_BUTTON_TEXT,
                    PRIMARY_BUTTON_CALLBACK, SECONDARY_BUTTON_CALLBACK, CLOSE_BUTTON_CALLBACK};
}

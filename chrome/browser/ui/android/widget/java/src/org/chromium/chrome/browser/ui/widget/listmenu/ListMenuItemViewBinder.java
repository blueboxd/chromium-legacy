// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.listmenu;

import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;

import org.chromium.chrome.browser.ui.widget.R;
import org.chromium.chrome.browser.ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the model of the ListMenuItem and the view.
 */
public class ListMenuItemViewBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        TextViewWithCompoundDrawables textView = (TextViewWithCompoundDrawables) view;
        if (propertyKey == ListMenuItemProperties.TITLE_ID) {
            textView.setText(model.get(ListMenuItemProperties.TITLE_ID));
        } else if (propertyKey == ListMenuItemProperties.START_ICON_ID) {
            int id = model.get(ListMenuItemProperties.START_ICON_ID);
            Drawable[] drawables = textView.getCompoundDrawablesRelative();
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    drawable, drawables[1], drawables[2], drawables[3]);
            if (id != 0) {
                // need more space between the start and the icon if icon is on the start.
                textView.setPaddingRelative(
                        view.getResources().getDimensionPixelOffset(R.dimen.menu_padding_start),
                        view.getPaddingTop(), view.getPaddingEnd(), view.getPaddingBottom());
            }
        } else if (propertyKey == ListMenuItemProperties.END_ICON_ID) {
            int id = model.get(ListMenuItemProperties.END_ICON_ID);
            Drawable[] drawables = textView.getCompoundDrawablesRelative();
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    drawables[0], drawables[1], drawable, drawables[3]);
        } else if (propertyKey == ListMenuItemProperties.TINT_COLOR_ID) {
            textView.setDrawableTintColor(ContextCompat.getColorStateList(
                    view.getContext(), model.get(ListMenuItemProperties.TINT_COLOR_ID)));
        }
    }
}

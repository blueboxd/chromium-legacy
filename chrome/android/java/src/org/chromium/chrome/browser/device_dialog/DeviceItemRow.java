// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.annotation.Nullable;
import android.text.TextUtils;

/**
 * A class representing one data row in the dialog.
 */
public class DeviceItemRow {
    public final String mKey;
    public String mDescription;
    public Drawable mIcon;
    public String mIconDescription;

    /**
     * Creates a device item row which can be shown in the dialog.
     *
     * @param key Item unique identifier.
     * @param description Item description.
     * @param icon Item icon.
     * @param iconDescription Item icon description.
     */
    public DeviceItemRow(String key, String description, @Nullable Drawable icon,
            @Nullable String iconDescription) {
        mKey = key;
        mDescription = description;
        mIcon = icon;
        mIconDescription = iconDescription;
    }

    /**
     * Returns true if all parameters match the corresponding member.
     *
     * @param key Expected item unique identifier.
     * @param description Expected item description.
     * @param icon Expected item icon.
     */
    public boolean hasSameContents(String key, String description, @Nullable Drawable icon,
            @Nullable String iconDescription) {
        if (!TextUtils.equals(mKey, key)) return false;
        if (!TextUtils.equals(mDescription, description)) return false;
        if (!TextUtils.equals(mIconDescription, iconDescription)) return false;

        if (icon == null ^ mIcon == null) return false;

        // On Android NMR1 and above, Drawable#getConstantState() always returns a different
        // value, so it does not make sense to compare it.
        // TODO(crbug.com/773043): Find a way to compare the icons.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1 && mIcon != null
                && !mIcon.getConstantState().equals(icon.getConstantState())) {
            return false;
        }

        return true;
    }
}

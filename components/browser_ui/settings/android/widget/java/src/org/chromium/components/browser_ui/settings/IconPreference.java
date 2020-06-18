// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.core.view.ViewCompat;
import androidx.preference.PreferenceViewHolder;

/**
 * A preference with a horizontally centered icon.
 */
public class IconPreference extends ChromeBasePreference {
    /**
     * Constructor for inflating from XML.
     */
    public IconPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        // Horizontally center the preference icon.

        // TODO(crbug.com/1095981): move this logic to ChromeBasePreference and
        // find a way to center it without a hard-coded padding.
        int padding = getContext().getResources().getDimensionPixelSize(R.dimen.pref_icon_padding);
        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        ViewCompat.setPaddingRelative(
                icon, padding, icon.getPaddingTop(), 0, icon.getPaddingBottom());
    }
}

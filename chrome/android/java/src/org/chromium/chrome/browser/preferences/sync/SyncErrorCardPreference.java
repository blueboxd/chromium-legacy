// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.sync;

import android.content.Context;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeBasePreferenceCompat;
import org.chromium.ui.UiUtils;

/**
 * A preference that displays hint message to resolve sync error. Click of it navigates user to
 * appropriate place to resolve error.
 */
public class SyncErrorCardPreference extends ChromeBasePreferenceCompat {
    /**
     * Constructor for inflating from XML.
     */
    public SyncErrorCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView title = (TextView) holder.findViewById(android.R.id.title);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        title.setTypeface(UiUtils.createRobotoMediumTypeface());
        title.setTextColor(ApiCompatibilityUtils.getColor(
                getContext().getResources(), R.color.input_underline_error_color));
    }
}

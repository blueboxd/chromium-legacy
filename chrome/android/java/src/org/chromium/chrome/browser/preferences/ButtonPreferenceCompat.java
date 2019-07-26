// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.Button;

import org.chromium.chrome.R;

/**
 * A {@link Preference} that provides button functionality.
 *
 * Preference.getOnPreferenceClickListener().onPreferenceClick() is called when the button is
 * clicked.
 */
public class ButtonPreferenceCompat extends Preference {
    /**
     * Constructor for inflating from XML
     */
    public ButtonPreferenceCompat(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.button_preference_layout);
        setWidgetLayoutResource(R.layout.button_preference_button);
        setSelectable(true);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        Button button = (Button) holder.findViewById(R.id.button_preference);
        button.setText(getTitle());
        button.setOnClickListener(v -> {
            if (getOnPreferenceClickListener() != null) {
                getOnPreferenceClickListener().onPreferenceClick(ButtonPreferenceCompat.this);
            }
        });

        // Prevent triggering an event after tapping any part of the view that is not the button.
        holder.itemView.setClickable(false);
    }
}

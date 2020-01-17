// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.autofill;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;

/**
 * A {@link Preference} that provides a clickable edit link as a widget.
 *
 * {@link OnPreferenceClickListener} is called when the link is clicked.
 */
public class AutofillEditLinkPreference extends Preference {
    /**
     * Constructor for inflating from XML.
     */
    public AutofillEditLinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
        setWidgetLayoutResource(R.layout.autofill_server_data_edit_link);
        setTitle(R.string.autofill_from_google_account_long);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        View button = holder.findViewById(R.id.preference_click_target);
        button.setClickable(true);
        button.setOnClickListener(v -> {
            if (getOnPreferenceClickListener() != null) {
                getOnPreferenceClickListener().onPreferenceClick(AutofillEditLinkPreference.this);
            }
        });
    }
}

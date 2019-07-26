// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v7.preference.PreferenceViewHolder;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

/**
 * A preference that displays informational text.
 */
public class TextMessagePreferenceCompat extends ChromeBasePreferenceCompat {
    /**
     * Constructor for inflating from XML.
     */
    public TextMessagePreferenceCompat(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        if (!TextUtils.isEmpty(getTitle())) {
            titleView.setVisibility(View.VISIBLE);
            titleView.setSingleLine(false);
            titleView.setMaxLines(Integer.MAX_VALUE);
            titleView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            titleView.setVisibility(View.GONE);
        }

        TextView summaryView = (TextView) holder.findViewById(android.R.id.summary);
        // No need to manually toggle visibility for summary - it is done in super.onBindView.
        summaryView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}

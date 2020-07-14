// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;

/**
 * A {@link Preference} for each Safety check element. In addition to the
 * functionality, provided by the {@link IconPreference}, has a status indicator
 * in the widget area that displays a progress bar or a status icon.
 */
public class SafetyCheckElementPreference extends ChromeBasePreference {
    private View mProgressBar;
    private ImageView mStatusView;

    /**
     * Creates a new object and sets the widget layout.
     */
    public SafetyCheckElementPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.safety_check_status);
    }

    /**
     * Gets triggered when the view elements are created.
     */
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mProgressBar = holder.findViewById(R.id.progress);
        mStatusView = (ImageView) holder.findViewById(R.id.status_view);
    }

    /**
     * Displays the progress bar.
     */
    void showProgressBar() {
        // Ignore if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            return;
        }
        mStatusView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.VISIBLE);
    }

    /**
     * Displays the status icon.
     * @param icon An icon to display.
     */
    void showStatusIcon(@DrawableRes int icon) {
        // Ignore if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            return;
        }
        mStatusView.setImageResource(icon);
        mProgressBar.setVisibility(View.GONE);
        mStatusView.setVisibility(View.VISIBLE);
    }

    /**
     * Hides anything in the status area.
     */
    void clearStatusIndicator() {
        // Ignore if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            return;
        }
        mStatusView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.GONE);
    }
}

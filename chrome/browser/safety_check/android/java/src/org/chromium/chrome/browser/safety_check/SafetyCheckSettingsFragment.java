// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Context;
import android.os.Bundle;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Settings fragment containing Safety check. This class represents a View in the MVC paradigm.
 */
public class SafetyCheckSettingsFragment extends PreferenceFragmentCompat {
    // Number of Safety check runs, after which the "NEW" label is no longer shown.
    public static final int SAFETY_CHECK_RUNS_SHOW_NEW_LABEL = 3;

    /** The "Check" button at the bottom that needs to be added after the View is inflated. */
    private ButtonCompat mCheckButton;

    private TextView mTimestampTextView;

    public static CharSequence getSafetyCheckSettingsElementTitle(Context context) {
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        if (preferenceManager.readInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER)
                < SAFETY_CHECK_RUNS_SHOW_NEW_LABEL) {
            // Show the styled "NEW" text if the user ran the Safety check less than 3 times.
            // TODO(crbug.com/1102827): remove the "NEW" label in M88 once the feature is no longer
            // "new".
            return SpanApplier.applySpans(context.getString(R.string.prefs_safety_check),
                    new SpanInfo("<new>", "</new>", new SuperscriptSpan(),
                            new RelativeSizeSpan(0.75f),
                            new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                    context.getResources(), R.color.default_text_color_blue))));
        } else {
            // Remove the "NEW" text and the trailing whitespace.
            return (CharSequence) (SpanApplier
                                           .removeSpanText(
                                                   context.getString(R.string.prefs_safety_check),
                                                   new SpanInfo("<new>", "</new>"))
                                           .toString()
                                           .trim());
        }
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_check_preferences);
        CharSequence safetyCheckTitle = SpanApplier.removeSpanText(
                getString(R.string.prefs_safety_check), new SpanInfo("<new>", "</new>"));
        // Remove the trailing whitespace left after deleting the "NEW" label.
        getActivity().setTitle(safetyCheckTitle.toString().trim());
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        // Add a button to the bottom of the preferences view.
        LinearLayout bottomView =
                (LinearLayout) inflater.inflate(R.layout.safety_check_bottom_elements, view, false);
        mCheckButton = (ButtonCompat) bottomView.findViewById(R.id.safety_check_button);
        mTimestampTextView = (TextView) bottomView.findViewById(R.id.safety_check_timestamp);
        view.addView(bottomView);
        return view;
    }

    /**
     * @return A {@link ButtonCompat} object for the Check button.
     */
    ButtonCompat getCheckButton() {
        return mCheckButton;
    }

    /**
     * @return A {@link TextView} object for the last run timestamp.
     */
    TextView getTimestampTextView() {
        return mTimestampTextView;
    }

    /**
     * Update the status string of a given Safety check element, e.g. Passwords.
     * @param key An android:key String corresponding to Safety check element.
     * @param statusString Resource ID of the new status string.
     */
    public void updateElementStatus(String key, int statusString) {
        if (statusString != 0) {
            updateElementStatus(key, getContext().getString(statusString));
        } else {
            updateElementStatus(key, "");
        }
    }

    /**
     * Update the status string of a given Safety check element, e.g. Passwords.
     * @param key An android:key String corresponding to Safety check element.
     * @param statusString The new status string.
     */
    public void updateElementStatus(String key, String statusString) {
        Preference p = findPreference(key);
        // If this is invoked before the preferences are created, do nothing.
        if (p == null) {
            return;
        }
        p.setSummary(statusString);
    }
}

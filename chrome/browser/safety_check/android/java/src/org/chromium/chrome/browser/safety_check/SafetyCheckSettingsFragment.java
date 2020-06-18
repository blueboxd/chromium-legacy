// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Fragment containing Safety check.
 */
public class SafetyCheckSettingsFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_check_preferences);
        getActivity().setTitle(R.string.prefs_safety_check);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);

        // Add a button to the bottom of the preferences view.
        ButtonCompat checkButton =
                (ButtonCompat) inflater.inflate(R.layout.safety_check_button, view, false);
        checkButton.setOnClickListener((View v) -> onSafetyCheckButtonClicked());
        view.addView(checkButton);
        return view;
    }

    private void onSafetyCheckButtonClicked() {}
}

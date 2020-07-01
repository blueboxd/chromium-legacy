// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Fragment containing Safety check.
 *
 * Note: it is required for {@link #setUpdatesClient(SafetyCheckUpdatesDelegate)} to have been
 * invoked before {@link #onCreatePreferences(Bundle, String)}.
 */
public class SafetyCheckSettingsFragment extends PreferenceFragmentCompat {
    private SafetyCheckController mController;
    private SafetyCheckModel mModel;
    private SafetyCheckUpdatesDelegate mUpdatesClient;

    /**
     * Initializes all the objects related to the preferences page.
     * Note: {@link #setUpdatesClient(SafetyCheckUpdatesDelegate)} should be invoked before this.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // The updates client should be set before this method is invoked.
        assert mUpdatesClient != null;
        // Create the model and the controller.
        mModel = new SafetyCheckModel(this);
        mController = new SafetyCheckController(mModel, mUpdatesClient);
        // Add all preferences and set the title.
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

    /**
     * Sets the client for interacting with Omaha for the updates check.
     * @param client An instance of a class implementing
     *               {@link SafetyCheckUpdatesDelegate}.
     */
    public void setUpdatesClient(SafetyCheckUpdatesDelegate client) {
        mUpdatesClient = client;
    }

    /**
     * Update the status string of a given Safety check element, e.g. Passwords.
     * @param key An android:key String corresponding to Safety check element.
     * @param statusString Resource ID of the new status string.
     */
    public void updateElementStatus(String key, int statusString) {
        Preference p = findPreference(key);
        p.setSummary(statusString);
    }

    /**
     * Gets triggered when the button for starting Safety check is pressed.
     */
    private void onSafetyCheckButtonClicked() {
        mController.performSafetyCheck();
    }
}

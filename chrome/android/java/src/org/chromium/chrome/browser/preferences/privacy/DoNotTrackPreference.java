// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreferenceCompat;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to manage 'Do Not Track' preference and to explain to the user what it does.
 */
public class DoNotTrackPreference extends PreferenceFragmentCompat {
    private static final String PREF_DO_NOT_TRACK_SWITCH = "do_not_track_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.do_not_track_preferences);
        getActivity().setTitle(R.string.do_not_track_title);

        ChromeSwitchPreferenceCompat doNotTrackSwitch =
                (ChromeSwitchPreferenceCompat) findPreference(PREF_DO_NOT_TRACK_SWITCH);

        boolean isDoNotTrackEnabled = PrefServiceBridge.getInstance().isDoNotTrackEnabled();
        doNotTrackSwitch.setChecked(isDoNotTrackEnabled);

        doNotTrackSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PrefServiceBridge.getInstance().setDoNotTrackEnabled((boolean) newValue);
            return true;
        });
    }
}

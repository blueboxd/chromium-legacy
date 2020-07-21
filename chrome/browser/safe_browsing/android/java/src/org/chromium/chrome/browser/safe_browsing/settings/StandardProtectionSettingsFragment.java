// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment containing standard protection settings.
 */
public class StandardProtectionSettingsFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.standard_protection_preferences);
        getActivity().setTitle(R.string.prefs_section_safe_browsing_title);
    }
}

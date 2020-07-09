// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment containing security settings.
 */
public class SecuritySettingsFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.security_preferences);
        getActivity().setTitle(R.string.prefs_security_title);
    }
}

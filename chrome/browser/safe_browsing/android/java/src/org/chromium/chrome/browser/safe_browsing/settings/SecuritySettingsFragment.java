// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.FragmentSettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment containing security settings.
 */
public class SecuritySettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsLauncher,
                   RadioButtonGroupSafeBrowsingPreference.OnSafeBrowsingModeDetailsRequested {
    @VisibleForTesting
    public static final String PREF_SAFE_BROWSING = "safe_browsing_radio_button_group";

    // An instance of SettingsLauncher that is used to launch Safe Browsing subsections.
    private SettingsLauncher mSettingsLauncher;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.security_preferences);
        getActivity().setTitle(R.string.prefs_security_title);

        RadioButtonGroupSafeBrowsingPreference safeBrowsingPreference =
                findPreference(PREF_SAFE_BROWSING);
        safeBrowsingPreference.init(SafeBrowsingBridge.getSafeBrowsingState(),
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED));
        safeBrowsingPreference.setSafeBrowsingModeDetailsRequestedListener(this);
        safeBrowsingPreference.setOnPreferenceChangeListener((preference, newValue) -> {
            @SafeBrowsingState
            int newState = (int) newValue;
            SafeBrowsingBridge.setSafeBrowsingState(newState);
            return true;
        });
    }

    @Override
    public void onSafeBrowsingModeDetailsRequested(@SafeBrowsingState int safeBrowsingState) {
        if (safeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION) {
            mSettingsLauncher.launchSettingsActivity(
                    getActivity(), EnhancedProtectionSettingsFragment.class);
        } else if (safeBrowsingState == SafeBrowsingState.STANDARD_PROTECTION) {
            mSettingsLauncher.launchSettingsActivity(
                    getActivity(), StandardProtectionSettingsFragment.class);
        } else {
            assert false : "Should not be reached";
        }
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }
}

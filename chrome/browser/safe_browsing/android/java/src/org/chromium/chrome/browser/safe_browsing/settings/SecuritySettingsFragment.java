// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.FragmentSettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

/**
 * Fragment containing security settings.
 * TODO(crbug.com/1097310): Rename it to SafeBrowsingSettingsFragment.
 */
public class SecuritySettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsLauncher,
                   RadioButtonGroupSafeBrowsingPreference.OnSafeBrowsingModeDetailsRequested,
                   Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    static final String PREF_TEXT_MANAGED = "text_managed";
    @VisibleForTesting
    static final String PREF_SAFE_BROWSING = "safe_browsing_radio_button_group";

    // An instance of SettingsLauncher that is used to launch Safe Browsing subsections.
    private SettingsLauncher mSettingsLauncher;
    private RadioButtonGroupSafeBrowsingPreference mSafeBrowsingPreference;

    /**
     * @return A summary that describes the current Safe Browsing state.
     */
    public static String getSafeBrowsingSummaryString(Context context) {
        @SafeBrowsingState
        int safeBrowsingState = SafeBrowsingBridge.getSafeBrowsingState();
        String safeBrowsingStateString = "";
        if (safeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION) {
            safeBrowsingStateString =
                    context.getString(R.string.safe_browsing_enhanced_protection_title);
        } else if (safeBrowsingState == SafeBrowsingState.STANDARD_PROTECTION) {
            safeBrowsingStateString =
                    context.getString(R.string.safe_browsing_standard_protection_title);
        } else if (safeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING) {
            return context.getString(R.string.prefs_safe_browsing_no_protection_summary);
        } else {
            assert false : "Should not be reached";
        }
        return context.getString(R.string.prefs_safe_browsing_summary, safeBrowsingStateString);
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.security_preferences);
        getActivity().setTitle(R.string.prefs_safe_browsing_title);

        ManagedPreferenceDelegate managedPreferenceDelegate = createManagedPreferenceDelegate();

        mSafeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsingPreference.init(SafeBrowsingBridge.getSafeBrowsingState(),
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED));
        mSafeBrowsingPreference.setSafeBrowsingModeDetailsRequestedListener(this);
        mSafeBrowsingPreference.setManagedPreferenceDelegate(managedPreferenceDelegate);
        mSafeBrowsingPreference.setOnPreferenceChangeListener(this);

        TextMessagePreference textManaged = findPreference(PREF_TEXT_MANAGED);
        textManaged.setManagedPreferenceDelegate(managedPreferenceDelegate);
        textManaged.setVisible(managedPreferenceDelegate.isPreferenceClickDisabledByPolicy(
                mSafeBrowsingPreference));
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

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_TEXT_MANAGED.equals(key) || PREF_SAFE_BROWSING.equals(key)) {
                return SafeBrowsingBridge.isSafeBrowsingManaged();
            } else {
                assert false : "Should not be reached.";
            }
            return false;
        };
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        assert PREF_SAFE_BROWSING.equals(key) : "Unexpected preference key.";
        @SafeBrowsingState
        int newState = (int) newValue;
        @SafeBrowsingState
        int currentState = SafeBrowsingBridge.getSafeBrowsingState();
        if (newState == currentState) {
            return true;
        }
        // If the user selects no protection from another Safe Browsing state, show a confirmation
        // dialog to double check if they want to select no protection.
        if (newState == SafeBrowsingState.NO_SAFE_BROWSING) {
            // The user hasn't confirmed to select no protection, keep the radio button / UI checked
            // state at the currently selected level.
            mSafeBrowsingPreference.setCheckedState(currentState);
            NoProtectionConfirmationDialog
                    .create(getContext(),
                            (didConfirm) -> {
                                if (didConfirm) {
                                    // The user has confirmed to select no protection, set Safe
                                    // Browsing pref to no protection, and change the radio button /
                                    // UI checked state to no protection.
                                    SafeBrowsingBridge.setSafeBrowsingState(
                                            SafeBrowsingState.NO_SAFE_BROWSING);
                                    mSafeBrowsingPreference.setCheckedState(
                                            SafeBrowsingState.NO_SAFE_BROWSING);
                                }
                                // No-ops if the user denies.
                            })
                    .show();
        } else {
            SafeBrowsingBridge.setSafeBrowsingState(newState);
        }
        return true;
    }
}

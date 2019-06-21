// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreferenceCompat;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to keep track of all downloads related preferences.
 */
public class DownloadPreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    public static final String PREF_LOCATION_CHANGE = "location_change";
    private static final String PREF_LOCATION_PROMPT_ENABLED = "location_prompt_enabled";
    private static final String PREF_PREFETCHING_ENABLED = "prefetching_enabled";

    private DownloadLocationPreference mLocationChangePref;
    private ChromeSwitchPreferenceCompat mLocationPromptEnabledPref;
    private ChromeSwitchPreferenceCompat mPrefetchingEnabled;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String s) {
        getActivity().setTitle(R.string.menu_downloads);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        mLocationPromptEnabledPref =
                (ChromeSwitchPreferenceCompat) findPreference(PREF_LOCATION_PROMPT_ENABLED);
        mLocationPromptEnabledPref.setOnPreferenceChangeListener(this);

        mLocationChangePref = (DownloadLocationPreference) findPreference(PREF_LOCATION_CHANGE);

        if (PrefetchConfiguration.isPrefetchingFlagEnabled()) {
            mPrefetchingEnabled =
                    (ChromeSwitchPreferenceCompat) findPreference(PREF_PREFETCHING_ENABLED);
            mPrefetchingEnabled.setOnPreferenceChangeListener(this);

            updatePrefetchSummary();
        } else {
            getPreferenceScreen().removePreference(findPreference(PREF_PREFETCHING_ENABLED));
        }
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof DownloadLocationPreference) {
            DownloadLocationPreferenceDialog dialogFragment =
                    DownloadLocationPreferenceDialog.newInstance(
                            (DownloadLocationPreference) preference);
            dialogFragment.setTargetFragment(this, 0);
            dialogFragment.show(getFragmentManager(), DownloadLocationPreferenceDialog.TAG);
        } else {
            super.onDisplayPreferenceDialog(preference);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        updateData();
    }

    private void updateData() {
        if (mLocationChangePref != null) {
            mLocationChangePref.updateSummary();
        }

        if (mLocationPromptEnabledPref != null) {
            // Location prompt is marked enabled if the prompt status is not DONT_SHOW.
            boolean isLocationPromptEnabled =
                    PrefServiceBridge.getInstance().getPromptForDownloadAndroid()
                    != DownloadPromptStatus.DONT_SHOW;
            mLocationPromptEnabledPref.setChecked(isLocationPromptEnabled);
        }

        if (mPrefetchingEnabled != null) {
            mPrefetchingEnabled.setChecked(PrefetchConfiguration.isPrefetchingEnabledInSettings());
            updatePrefetchSummary();
        }
    }

    private void updatePrefetchSummary() {
        // The summary text should remain empty if mPrefetchingEnabled is switched off so it is only
        // updated when the setting is on.
        if (PrefetchConfiguration.isPrefetchingEnabled()) {
            mPrefetchingEnabled.setSummaryOn("");
        } else if (PrefetchConfiguration.isPrefetchingEnabledInSettings()) {
            // If prefetching is enabled by the user but isPrefetchingEnabled() returned false, we
            // know that prefetching is forbidden by the server.
            if (PrefetchConfiguration.isEnabledByServerUnknown()) {
                mPrefetchingEnabled.setSummaryOn(
                        R.string.download_settings_prefetch_maybe_unavailable_description);
            } else {
                mPrefetchingEnabled.setSummaryOn(
                        R.string.download_settings_prefetch_unavailable_description);
            }
        }
    }

    // Preference.OnPreferenceChangeListener implementation.

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_LOCATION_PROMPT_ENABLED.equals(preference.getKey())) {
            if ((boolean) newValue) {
                // Only update if the interstitial has been shown before.
                if (PrefServiceBridge.getInstance().getPromptForDownloadAndroid()
                        != DownloadPromptStatus.SHOW_INITIAL) {
                    PrefServiceBridge.getInstance().setPromptForDownloadAndroid(
                            DownloadPromptStatus.SHOW_PREFERENCE);
                }
            } else {
                PrefServiceBridge.getInstance().setPromptForDownloadAndroid(
                        DownloadPromptStatus.DONT_SHOW);
            }
        } else if (PREF_PREFETCHING_ENABLED.equals(preference.getKey())) {
            PrefetchConfiguration.setPrefetchingEnabledInSettings((boolean) newValue);
            updatePrefetchSummary();
        }
        return true;
    }
}

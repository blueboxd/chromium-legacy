// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreferenceCompat;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegateCompat;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Fragment to keep track of the all the privacy related preferences.
 */
public class PrivacyPreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_NETWORK_PREDICTIONS = "preload_pages";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";
    private static final String PREF_DO_NOT_TRACK = "do_not_track";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";

    private ManagedPreferenceDelegateCompat mManagedPreferenceDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PrivacyPreferencesManager privacyPrefManager = PrivacyPreferencesManager.getInstance();
        privacyPrefManager.migrateNetworkPredictionPreferences();
        PreferenceUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);
        getActivity().setTitle(R.string.prefs_privacy);
        setHasOptionsMenu(true);
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        ChromeBaseCheckBoxPreferenceCompat canMakePaymentPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        ChromeBaseCheckBoxPreferenceCompat networkPredictionPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_NETWORK_PREDICTIONS);
        networkPredictionPref.setChecked(prefServiceBridge.getNetworkPredictionEnabled());
        networkPredictionPref.setOnPreferenceChangeListener(this);
        networkPredictionPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
        NoUnderlineClickableSpan linkSpan = new NoUnderlineClickableSpan(getResources(), view -> {
            PreferencesLauncher.launchSettingsPageCompat(getActivity(),
                    SyncAndServicesPreferences.class,
                    SyncAndServicesPreferences.createArguments(false));
        });
        syncAndServicesLink.setSummary(
                SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        updateSummaries();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            PrefServiceBridge.getInstance().setNetworkPredictionEnabled((boolean) newValue);
        }

        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSummaries();
    }

    /**
     * Updates the summaries for several preferences.
     */
    public void updateSummaries() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        CharSequence textOn = getActivity().getResources().getText(R.string.text_on);
        CharSequence textOff = getActivity().getResources().getText(R.string.text_off);

        CheckBoxPreference canMakePaymentPref =
                (CheckBoxPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        if (canMakePaymentPref != null) {
            canMakePaymentPref.setChecked(
                    prefServiceBridge.getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED));
        }

        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);
        if (doNotTrackPref != null) {
            doNotTrackPref.setSummary(prefServiceBridge.isDoNotTrackEnabled() ? textOn : textOff);
        }

        Preference usageStatsPref = findPreference(PREF_USAGE_STATS);
        if (usageStatsPref != null) {
            if (BuildInfo.isAtLeastQ() && prefServiceBridge.getBoolean(Pref.USAGE_STATS_ENABLED)) {
                usageStatsPref.setOnPreferenceClickListener(preference -> {
                    UsageStatsConsentDialog
                            .create(getActivity(), true,
                                    (didConfirm) -> {
                                        if (didConfirm) {
                                            updateSummaries();
                                        }
                                    })
                            .show();
                    return true;
                });
            } else {
                getPreferenceScreen().removePreference(usageStatsPref);
            }
        }
    }

    private ManagedPreferenceDelegateCompat createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            if (PREF_NETWORK_PREDICTIONS.equals(key)) {
                return prefs.isNetworkPredictionManaged();
            }
            return false;
        };
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_privacy),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceScreen;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreferenceCompat;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegateCompat;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Fragment to keep track of the all the privacy related preferences.
 */
public class PrivacyPreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    private static final String PREF_NAVIGATION_ERROR = "navigation_error";
    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_SAFE_BROWSING_SCOUT_REPORTING =
            "safe_browsing_scout_reporting";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";
    private static final String PREF_NETWORK_PREDICTIONS = "network_predictions";
    private static final String PREF_DO_NOT_TRACK = "do_not_track";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_CLEAR_BROWSING_DATA = "clear_browsing_data";
    private static final String PREF_SYNC_AND_SERVICES_LINK_DIVIDER =
            "sync_and_services_link_divider";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";

    private ManagedPreferenceDelegateCompat mManagedPreferenceDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PrivacyPreferencesManager privacyPrefManager = PrivacyPreferencesManager.getInstance();
        privacyPrefManager.migrateNetworkPredictionPreferences();
        PreferenceUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);
        getActivity().setTitle(R.string.prefs_privacy);
        setHasOptionsMenu(true);
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        PreferenceScreen preferenceScreen = getPreferenceScreen();

        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        ChromeBaseCheckBoxPreferenceCompat canMakePaymentPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        ChromeBaseCheckBoxPreferenceCompat networkPredictionPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_NETWORK_PREDICTIONS);
        networkPredictionPref.setChecked(prefServiceBridge.getNetworkPredictionEnabled());
        networkPredictionPref.setOnPreferenceChangeListener(this);
        networkPredictionPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)) {
            // Remove preferences that were migrated to SyncAndServicesPreferences.
            preferenceScreen.removePreference(findPreference(PREF_NAVIGATION_ERROR));
            preferenceScreen.removePreference(findPreference(PREF_SEARCH_SUGGESTIONS));
            preferenceScreen.removePreference(findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING));
            preferenceScreen.removePreference(findPreference(PREF_SAFE_BROWSING));
            preferenceScreen.removePreference(findPreference(PREF_CONTEXTUAL_SEARCH));
            preferenceScreen.removePreference(findPreference(PREF_USAGE_AND_CRASH_REPORTING));

            // TODO(https://crbug.com/846376): Update strings in XML after UNIFIED_CONSENT launch.
            networkPredictionPref.setTitle(R.string.preload_pages_title);
            networkPredictionPref.setSummary(R.string.preload_pages_summary);

            // Put networkPredictionPref after canMakePaymentPref by overriding order value.
            // However, calling setOrder doesn't change existing order if Preference has already
            // been added to PreferenceGroup. Remove and re-add it to work around this.
            // TODO(https://crbug.com/846376): Reorder prefs in XML after UNIFIED_CONSENT launch.
            preferenceScreen.removePreference(networkPredictionPref);
            networkPredictionPref.setOrder(canMakePaymentPref.getOrder());
            preferenceScreen.addPreference(networkPredictionPref);

            Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
            NoUnderlineClickableSpan linkSpan =
                    new NoUnderlineClickableSpan(getResources(), view -> {
                        PreferencesLauncher.launchSettingsPage(getActivity(),
                                SyncAndServicesPreferences.class,
                                SyncAndServicesPreferences.createArguments(false));
                    });
            syncAndServicesLink.setSummary(
                    SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link),
                            new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

            updateSummaries();
            return;
        }
        preferenceScreen.removePreference(findPreference(PREF_SYNC_AND_SERVICES_LINK_DIVIDER));
        preferenceScreen.removePreference(findPreference(PREF_SYNC_AND_SERVICES_LINK));

        ChromeBaseCheckBoxPreferenceCompat navigationErrorPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_NAVIGATION_ERROR);
        navigationErrorPref.setOnPreferenceChangeListener(this);
        navigationErrorPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        ChromeBaseCheckBoxPreferenceCompat searchSuggestionsPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_SEARCH_SUGGESTIONS);
        searchSuggestionsPref.setOnPreferenceChangeListener(this);
        searchSuggestionsPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        if (!ContextualSearchFieldTrial.isEnabled()) {
            preferenceScreen.removePreference(findPreference(PREF_CONTEXTUAL_SEARCH));
        }

        // Listen to changes to the Extended Reporting pref.
        ChromeBaseCheckBoxPreferenceCompat scoutReportingPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(
                        PREF_SAFE_BROWSING_SCOUT_REPORTING);
        scoutReportingPref.setOnPreferenceChangeListener(this);
        scoutReportingPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        ChromeBaseCheckBoxPreferenceCompat safeBrowsingPref =
                (ChromeBaseCheckBoxPreferenceCompat) findPreference(PREF_SAFE_BROWSING);
        safeBrowsingPref.setOnPreferenceChangeListener(this);
        safeBrowsingPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        updateSummaries();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            PrefServiceBridge.getInstance().setSearchSuggestEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING.equals(key)) {
            PrefServiceBridge.getInstance().setSafeBrowsingEnabled((boolean) newValue);
        } else if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
            PrefServiceBridge.getInstance().setSafeBrowsingExtendedReportingEnabled(
                    (boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            PrefServiceBridge.getInstance().setNetworkPredictionEnabled((boolean) newValue);
            recordNetworkPredictionEnablingUMA((boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            PrefServiceBridge.getInstance().setResolveNavigationErrorEnabled((boolean) newValue);
        } else if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        }

        return true;
    }

    private void recordNetworkPredictionEnablingUMA(boolean enabled) {
        // Report user turning on and off NetworkPrediction.
        RecordHistogram.recordBooleanHistogram("PrefService.NetworkPredictionEnabled", enabled);
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

        PrivacyPreferencesManager privacyPrefManager = PrivacyPreferencesManager.getInstance();

        CharSequence textOn = getActivity().getResources().getText(R.string.text_on);
        CharSequence textOff = getActivity().getResources().getText(R.string.text_off);

        CheckBoxPreference navigationErrorPref = (CheckBoxPreference) findPreference(
                PREF_NAVIGATION_ERROR);
        if (navigationErrorPref != null) {
            navigationErrorPref.setChecked(
                    prefServiceBridge.isResolveNavigationErrorEnabled());
        }

        CheckBoxPreference searchSuggestionsPref = (CheckBoxPreference) findPreference(
                PREF_SEARCH_SUGGESTIONS);
        if (searchSuggestionsPref != null) {
            searchSuggestionsPref.setChecked(prefServiceBridge.isSearchSuggestEnabled());
        }

        CheckBoxPreference extendedReportingPref =
                (CheckBoxPreference) findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING);
        if (extendedReportingPref != null) {
            extendedReportingPref.setChecked(
                    prefServiceBridge.isSafeBrowsingExtendedReportingEnabled());
        }

        CheckBoxPreference safeBrowsingPref =
                (CheckBoxPreference) findPreference(PREF_SAFE_BROWSING);
        if (safeBrowsingPref != null) {
            safeBrowsingPref.setChecked(prefServiceBridge.isSafeBrowsingEnabled());
        }

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

        Preference contextualPref = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (contextualPref != null) {
            boolean isContextualSearchEnabled = !prefServiceBridge.isContextualSearchDisabled();
            contextualPref.setSummary(isContextualSearchEnabled ? textOn : textOff);
        }

        Preference usageAndCrashPref = findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        if (usageAndCrashPref != null) {
            usageAndCrashPref.setSummary(
                    privacyPrefManager.isUsageAndCrashReportingPermittedByUser() ? textOn
                                                                                 : textOff);
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
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return prefs.isResolveNavigationErrorManaged();
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return prefs.isSearchSuggestManaged();
            }
            if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
                return prefs.isSafeBrowsingExtendedReportingManaged();
            }
            if (PREF_SAFE_BROWSING.equals(key)) {
                return prefs.isSafeBrowsingManaged();
            }
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

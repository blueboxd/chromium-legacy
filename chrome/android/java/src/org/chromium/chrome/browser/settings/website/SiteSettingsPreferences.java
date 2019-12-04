// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.website;

import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.settings.LocationSettings;
import org.chromium.chrome.browser.settings.SettingsUtils;
import org.chromium.chrome.browser.settings.website.SiteSettingsCategory.Type;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentSwitches;

import java.util.ArrayList;
import java.util.List;

/**
 * The main Site Settings screen, which shows all the site settings categories: All sites, Location,
 * Microphone, etc. By clicking into one of these categories, the user can see or and modify
 * permissions that have been granted to websites, as well as enable or disable permissions
 * browser-wide.
 */
public class SiteSettingsPreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    // The keys for each category shown on the Site Settings page
    // are defined in the SiteSettingsCategory.

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.site_settings_preferences);
        getActivity().setTitle(R.string.prefs_site_settings);

        configurePreferences();
        updatePreferenceStates();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        setDivider(null);
    }

    private Preference findPreference(@Type int type) {
        return findPreference(SiteSettingsCategory.preferenceKey(type));
    }

    private void configurePreferences() {
        // TODO(csharrison): Remove this condition once the experimental UI lands. It is not
        // great to dynamically remove the preference in this way.
        if (!SiteSettingsCategory.adsCategoryEnabled()) {
            getPreferenceScreen().removePreference(findPreference(Type.ADS));
        }
        CommandLine commandLine = CommandLine.getInstance();
        if (!commandLine.hasSwitch(ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES)) {
            getPreferenceScreen().removePreference(findPreference(Type.BLUETOOTH_SCANNING));
        }
        if (!ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC)) {
            getPreferenceScreen().removePreference(findPreference(Type.NFC));
        }
    }

    private void updatePreferenceStates() {
        // Preferences that navigate to Website Settings.
        List<Integer> websitePrefs = new ArrayList<Integer>();
        if (SiteSettingsCategory.adsCategoryEnabled()) {
            websitePrefs.add(Type.ADS);
        }
        websitePrefs.add(Type.AUTOMATIC_DOWNLOADS);
        websitePrefs.add(Type.BACKGROUND_SYNC);
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES)) {
            websitePrefs.add(Type.BLUETOOTH_SCANNING);
        }
        websitePrefs.add(Type.CAMERA);
        websitePrefs.add(Type.CLIPBOARD);
        websitePrefs.add(Type.COOKIES);
        websitePrefs.add(Type.JAVASCRIPT);
        websitePrefs.add(Type.DEVICE_LOCATION);
        websitePrefs.add(Type.MICROPHONE);
        if (ContentFeatureList.isEnabled(ContentFeatureList.WEB_NFC)) {
            websitePrefs.add(Type.NFC);
        }
        websitePrefs.add(Type.NOTIFICATIONS);
        websitePrefs.add(Type.POPUPS);
        websitePrefs.add(Type.SENSORS);
        websitePrefs.add(Type.PROTECTED_MEDIA);
        websitePrefs.add(Type.SOUND);
        websitePrefs.add(Type.USB);

        // Initialize the summary and icon for all preferences that have an
        // associated content settings entry.
        for (@Type int prefCategory : websitePrefs) {
            Preference p = findPreference(prefCategory);
            int contentType = SiteSettingsCategory.contentSettingsType(prefCategory);
            boolean requiresTriStateSetting =
                    WebsitePreferenceBridge.requiresTriStateContentSetting(contentType);

            boolean checked = false;
            @ContentSettingValues
            int setting = ContentSettingValues.DEFAULT;

            if (prefCategory == Type.DEVICE_LOCATION) {
                checked = LocationSettings.getInstance().areAllLocationSettingsEnabled();
            } else if (requiresTriStateSetting) {
                setting = WebsitePreferenceBridge.getContentSetting(contentType);
            } else {
                checked = WebsitePreferenceBridge.isCategoryEnabled(contentType);
            }

            p.setTitle(ContentSettingsResources.getTitle(contentType));
            p.setOnPreferenceClickListener(this);

            if ((Type.CAMERA == prefCategory || Type.MICROPHONE == prefCategory
                        || Type.NOTIFICATIONS == prefCategory)
                    && SiteSettingsCategory.createFromType(prefCategory)
                               .showPermissionBlockedMessage(getActivity())) {
                // Show 'disabled' message when permission is not granted in Android.
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, false));
            } else if (Type.COOKIES == prefCategory && checked
                    && PrefServiceBridge.getInstance().getBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES)) {
                p.setSummary(ContentSettingsResources.getCookieAllowedExceptThirdPartySummary());
            } else if (Type.DEVICE_LOCATION == prefCategory && checked
                    && WebsitePreferenceBridge.isLocationAllowedByPolicy()) {
                p.setSummary(ContentSettingsResources.getGeolocationAllowedSummary());
            } else if (Type.CLIPBOARD == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getClipboardBlockedListSummary());
            } else if (Type.ADS == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getAdsBlockedListSummary());
            } else if (Type.SOUND == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getSoundBlockedListSummary());
            } else if (requiresTriStateSetting) {
                p.setSummary(ContentSettingsResources.getCategorySummary(setting));
            } else {
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, checked));
            }

            if (p.isEnabled()) {
                p.setIcon(SettingsUtils.getTintedIcon(
                        getActivity(), ContentSettingsResources.getIcon(contentType)));
            } else {
                p.setIcon(ContentSettingsResources.getDisabledIcon(contentType, getResources()));
            }
        }

        Preference p = findPreference(Type.ALL_SITES);
        if (p != null) p.setOnPreferenceClickListener(this);
        // TODO(finnur): Re-move this for Storage once it can be moved to the 'Usage' menu.
        p = findPreference(Type.USE_STORAGE);
        if (p != null) p.setOnPreferenceClickListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceStates();
    }

    // OnPreferenceClickListener:

    @Override
    public boolean onPreferenceClick(Preference preference) {
        preference.getExtras().putString(
                SingleCategoryPreferences.EXTRA_CATEGORY, preference.getKey());
        preference.getExtras().putString(SingleCategoryPreferences.EXTRA_TITLE,
                preference.getTitle().toString());
        return false;
    }
}

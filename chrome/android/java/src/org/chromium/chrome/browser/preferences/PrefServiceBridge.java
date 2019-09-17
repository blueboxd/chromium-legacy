// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.SharedPreferences;
import android.util.Log;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.preferences.languages.LanguageItem;
import org.chromium.chrome.browser.preferences.website.ContentSettingException;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * PrefServiceBridge is a singleton which provides access to some native preferences. Ideally
 * preferences should be grouped with their relevant functionality but this is a grab-bag for other
 * preferences.
 */
public class PrefServiceBridge {
    // These values must match the native enum values in
    // SupervisedUserURLFilter::FilteringBehavior
    public static final int SUPERVISED_USER_FILTERING_ALLOW = 0;
    public static final int SUPERVISED_USER_FILTERING_WARN = 1;
    public static final int SUPERVISED_USER_FILTERING_BLOCK = 2;

    private static final String MIGRATION_PREF_KEY = "PrefMigrationVersion";
    private static final int MIGRATION_CURRENT_VERSION = 4;

    /** The android permissions associated with requesting location. */
    private static final String[] LOCATION_PERMISSIONS = {
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION};
    /** The android permissions associated with requesting access to the camera. */
    private static final String[] CAMERA_PERMISSIONS = {android.Manifest.permission.CAMERA};
    /** The android permissions associated with requesting access to the microphone. */
    private static final String[] MICROPHONE_PERMISSIONS = {
            android.Manifest.permission.RECORD_AUDIO};
    /** Signifies there are no permissions associated. */
    private static final String[] EMPTY_PERMISSIONS = {};

    private static final String LOG_TAG = "PrefServiceBridge";

    // Constants related to the Contextual Search preference.
    private static final String CONTEXTUAL_SEARCH_DISABLED = "false";
    private static final String CONTEXTUAL_SEARCH_ENABLED = "true";

    /**
     * Structure that holds all the version information about the current Chrome browser.
     */
    public static class AboutVersionStrings {
        private final String mApplicationVersion;
        private final String mOSVersion;

        private AboutVersionStrings(String applicationVersion, String osVersion) {
            mApplicationVersion = applicationVersion;
            mOSVersion = osVersion;
        }

        public String getApplicationVersion() {
            return mApplicationVersion;
        }

        public String getOSVersion() {
            return mOSVersion;
        }
    }

    @CalledByNative
    private static AboutVersionStrings createAboutVersionStrings(String applicationVersion,
            String osVersion) {
        return new AboutVersionStrings(applicationVersion, osVersion);
    }

    // Singleton constructor. Do not call directly unless for testing purpose.
    @VisibleForTesting
    protected PrefServiceBridge() {}

    private static PrefServiceBridge sInstance;

    /**
     * @return The singleton preferences object.
     */
    public static PrefServiceBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new PrefServiceBridge();

            // TODO(wnwen): Check while refactoring TemplateUrlService whether this belongs here.
            // This is necessary as far as ensuring that TemplateUrlService is loaded at some point.
            // Put initialization here to make instantiation in unit tests easier.
            TemplateUrlServiceFactory.get().load();
        }
        return sInstance;
    }

    /**
     * @return Whether the preferences have been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is enabled.
     */
    public boolean getBoolean(@Pref int preference) {
        return PrefServiceBridgeJni.get().getBoolean(PrefServiceBridge.this, preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setBoolean(@Pref int preference, boolean value) {
        PrefServiceBridgeJni.get().setBoolean(PrefServiceBridge.this, preference, value);
    }

    /**
     * Migrates (synchronously) the preferences to the most recent version.
     */
    public void migratePreferences() {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        int currentVersion = preferences.getInt(MIGRATION_PREF_KEY, 0);
        if (currentVersion == MIGRATION_CURRENT_VERSION) return;
        if (currentVersion > MIGRATION_CURRENT_VERSION) {
            Log.e(LOG_TAG, "Saved preferences version is newer than supported.  Attempting to "
                    + "run an older version of Chrome without clearing data is unsupported and "
                    + "the results may be unpredictable.");
        }

        if (currentVersion < 1) {
            PrefServiceBridgeJni.get().migrateJavascriptPreference(PrefServiceBridge.this);
        }
        // Steps 2,3,4 intentionally skipped.
        preferences.edit().putInt(MIGRATION_PREF_KEY, MIGRATION_CURRENT_VERSION).apply();
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingEnabled(int contentSettingsType) {
        return PrefServiceBridgeJni.get().isContentSettingEnabled(
                PrefServiceBridge.this, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingManaged(int contentSettingsType) {
        return PrefServiceBridgeJni.get().isContentSettingManaged(
                PrefServiceBridge.this, contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public void setContentSettingEnabled(int contentSettingsType, boolean enabled) {
        PrefServiceBridgeJni.get().setContentSettingEnabled(
                PrefServiceBridge.this, contentSettingsType, enabled);
    }

    /**
     * Returns all the currently saved exceptions for a given content settings type.
     * @param contentSettingsType The type to fetch exceptions for.
     */
    public List<ContentSettingException> getContentSettingsExceptions(int contentSettingsType) {
        List<ContentSettingException> list = new ArrayList<ContentSettingException>();
        PrefServiceBridgeJni.get().getContentSettingsExceptions(
                PrefServiceBridge.this, contentSettingsType, list);
        return list;
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(ArrayList<ContentSettingException> list,
            int contentSettingsType, String pattern, int contentSetting, String source) {
        ContentSettingException exception =
                new ContentSettingException(contentSettingsType, pattern, contentSetting, source);
        list.add(exception);
    }

    @CalledByNative
    private static void addNewLanguageItemToList(List<LanguageItem> list, String code,
            String displayName, String nativeDisplayName, boolean supportTranslate) {
        list.add(new LanguageItem(code, displayName, nativeDisplayName, supportTranslate));
    }

    @CalledByNative
    private static void copyStringArrayToList(List<String> list, String[] source) {
        list.addAll(Arrays.asList(source));
    }

    /**
     * Return the list of android permission strings for a given {@link ContentSettingsType}.  If
     * there is no permissions associated with the content setting, then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the android permission for.
     * @return The android permissions for the given content setting.
     */
    @CalledByNative
    public static String[] getAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                return Arrays.copyOf(LOCATION_PERMISSIONS, LOCATION_PERMISSIONS.length);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    /**
     * @return Whether cookies acceptance is modifiable by the user
     */
    public boolean isAcceptCookiesUserModifiable() {
        return PrefServiceBridgeJni.get().getAcceptCookiesUserModifiable(PrefServiceBridge.this);
    }

    /**
     * @return Whether cookies acceptance is configured by the user's custodian
     * (for supervised users).
     */
    public boolean isAcceptCookiesManagedByCustodian() {
        return PrefServiceBridgeJni.get().getAcceptCookiesManagedByCustodian(
                PrefServiceBridge.this);
    }

    public boolean isBlockThirdPartyCookiesEnabled() {
        return PrefServiceBridgeJni.get().getBlockThirdPartyCookiesEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether third-party cookie blocking is configured by policy
     */
    public boolean isBlockThirdPartyCookiesManaged() {
        return PrefServiceBridgeJni.get().getBlockThirdPartyCookiesManaged(PrefServiceBridge.this);
    }

    public boolean isRememberPasswordsEnabled() {
        return PrefServiceBridgeJni.get().getRememberPasswordsEnabled(PrefServiceBridge.this);
    }

    public boolean isPasswordManagerAutoSigninEnabled() {
        return PrefServiceBridgeJni.get().getPasswordManagerAutoSigninEnabled(
                PrefServiceBridge.this);
    }

    public boolean isPasswordLeakDetectionEnabled() {
        return PrefServiceBridgeJni.get().getPasswordLeakDetectionEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether password storage is configured by policy
     */
    public boolean isRememberPasswordsManaged() {
        return PrefServiceBridgeJni.get().getRememberPasswordsManaged(PrefServiceBridge.this);
    }

    public boolean isPasswordManagerAutoSigninManaged() {
        return PrefServiceBridgeJni.get().getPasswordManagerAutoSigninManaged(
                PrefServiceBridge.this);
    }

    /**
     * @return Whether leak detection is enabled/disabled by policy
     */
    public boolean isPasswordLeakDetectionManaged() {
        return PrefServiceBridgeJni.get().getPasswordLeakDetectionManaged(PrefServiceBridge.this);
    }

    /**
     * @return Whether vibration is enabled for notifications.
     */
    public boolean isNotificationsVibrateEnabled() {
        return PrefServiceBridgeJni.get().getNotificationsVibrateEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether geolocation information can be shared with content.
     */
    public boolean isAllowLocationEnabled() {
        return PrefServiceBridgeJni.get().getAllowLocationEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public boolean isLocationAllowedByPolicy() {
        return PrefServiceBridgeJni.get().getLocationAllowedByPolicy(PrefServiceBridge.this);
    }

    /**
     * @return Whether the location preference is modifiable by the user.
     */
    public boolean isAllowLocationUserModifiable() {
        return PrefServiceBridgeJni.get().getAllowLocationUserModifiable(PrefServiceBridge.this);
    }

    /**
     * @return Whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public boolean isAllowLocationManagedByCustodian() {
        return PrefServiceBridgeJni.get().getAllowLocationManagedByCustodian(
                PrefServiceBridge.this);
    }

    /**
     * @return Whether Do Not Track is enabled
     */
    public boolean isDoNotTrackEnabled() {
        return PrefServiceBridgeJni.get().getDoNotTrackEnabled(PrefServiceBridge.this);
    }

    public boolean getPasswordEchoEnabled() {
        return PrefServiceBridgeJni.get().getPasswordEchoEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public boolean isFirstRunEulaAccepted() {
        return PrefServiceBridgeJni.get().getFirstRunEulaAccepted(PrefServiceBridge.this);
    }

    /**
     * @return Whether JavaScript is managed by policy.
     */
    public boolean javaScriptManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    }

    /**
     * @return true if background sync is managed by policy.
     */
    public boolean isBackgroundSyncManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC);
    }

    /**
     * @return true if automatic downloads is managed by policy.
     */
    public boolean isAutomaticDownloadsManaged() {
        return isContentSettingManaged(
                ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS);
    }

    /**
     * Sets the preference that controls translate
     */
    public void setTranslateEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setTranslateEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
    public void setEulaAccepted() {
        PrefServiceBridgeJni.get().setEulaAccepted(PrefServiceBridge.this);
    }

    /**
     * @return the last account id associated with sync.
     */
    public String getSyncLastAccountId() {
        return PrefServiceBridgeJni.get().getSyncLastAccountId(PrefServiceBridge.this);
    }

    /**
     * @return the last account username associated with sync.
     */
    public String getSyncLastAccountName() {
        return PrefServiceBridgeJni.get().getSyncLastAccountName(PrefServiceBridge.this);
    }

    /**
     * @return Whether Search Suggest is enabled.
     */
    public boolean isSearchSuggestEnabled() {
        return PrefServiceBridgeJni.get().getSearchSuggestEnabled(PrefServiceBridge.this);
    }

    /**
     * Sets whether search suggest should be enabled.
     */
    public void setSearchSuggestEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setSearchSuggestEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether Search Suggest is configured by policy.
     */
    public boolean isSearchSuggestManaged() {
        return PrefServiceBridgeJni.get().getSearchSuggestManaged(PrefServiceBridge.this);
    }

    /**
     * @return the Contextual Search preference.
     */
    public String getContextualSearchPreference() {
        return PrefServiceBridgeJni.get().getContextualSearchPreference(PrefServiceBridge.this);
    }

    /**
     * Sets the Contextual Search preference.
     * @param prefValue one of "", CONTEXTUAL_SEARCH_ENABLED or CONTEXTUAL_SEARCH_DISABLED.
     */
    public void setContextualSearchPreference(String prefValue) {
        PrefServiceBridgeJni.get().setContextualSearchPreference(PrefServiceBridge.this, prefValue);
    }

    /**
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    public boolean isContextualSearchDisabled() {
        return getContextualSearchPreference().equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature is disabled by policy.
     */
    public boolean isContextualSearchDisabledByPolicy() {
        return PrefServiceBridgeJni.get().getContextualSearchPreferenceIsManaged(
                       PrefServiceBridge.this)
                && isContextualSearchDisabled();
    }

    /**
     * @return Whether the Contextual Search feature is uninitialized (preference unset by the
     *         user).
     */
    public boolean isContextualSearchUninitialized() {
        return getContextualSearchPreference().isEmpty();
    }

    /**
     * @param enabled Whether Contextual Search should be enabled.
     */
    public void setContextualSearchState(boolean enabled) {
        setContextualSearchPreference(enabled
                ? CONTEXTUAL_SEARCH_ENABLED : CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is currently enabled.
     */
    public boolean isSafeBrowsingExtendedReportingEnabled() {
        return PrefServiceBridgeJni.get().getSafeBrowsingExtendedReportingEnabled(
                PrefServiceBridge.this);
    }

    /**
     * @param enabled Whether Safe Browsing Extended Reporting should be enabled.
     */
    public void setSafeBrowsingExtendedReportingEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setSafeBrowsingExtendedReportingEnabled(
                PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is managed
     */
    public boolean isSafeBrowsingExtendedReportingManaged() {
        return PrefServiceBridgeJni.get().getSafeBrowsingExtendedReportingManaged(
                PrefServiceBridge.this);
    }

    /**
     * @return Whether Safe Browsing is currently enabled.
     */
    public boolean isSafeBrowsingEnabled() {
        return PrefServiceBridgeJni.get().getSafeBrowsingEnabled(PrefServiceBridge.this);
    }

    /**
     * @param enabled Whether Safe Browsing should be enabled.
     */
    public void setSafeBrowsingEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setSafeBrowsingEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether Safe Browsing is managed
     */
    public boolean isSafeBrowsingManaged() {
        return PrefServiceBridgeJni.get().getSafeBrowsingManaged(PrefServiceBridge.this);
    }

    /**
     * @return Whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration. See http://crbug.com/334602
     */
    public boolean obsoleteNetworkPredictionOptionsHasUserSetting() {
        return PrefServiceBridgeJni.get().obsoleteNetworkPredictionOptionsHasUserSetting(
                PrefServiceBridge.this);
    }

    /**
     * @return Network predictions preference.
     */
    public boolean getNetworkPredictionEnabled() {
        return PrefServiceBridgeJni.get().getNetworkPredictionEnabled(PrefServiceBridge.this);
    }

    /**
     * Sets network predictions preference.
     */
    public void setNetworkPredictionEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setNetworkPredictionEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether Network Predictions is configured by policy.
     */
    public boolean isNetworkPredictionManaged() {
        return PrefServiceBridgeJni.get().getNetworkPredictionManaged(PrefServiceBridge.this);
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    public boolean canPrefetchAndPrerender() {
        return PrefServiceBridgeJni.get().canPrefetchAndPrerender(PrefServiceBridge.this);
    }

    /**
     * @return Whether the web service to resolve navigation error is enabled.
     */
    public boolean isResolveNavigationErrorEnabled() {
        return PrefServiceBridgeJni.get().getResolveNavigationErrorEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether the web service to resolve navigation error is configured by policy.
     */
    public boolean isResolveNavigationErrorManaged() {
        return PrefServiceBridgeJni.get().getResolveNavigationErrorManaged(PrefServiceBridge.this);
    }

    /**
     * @return true if translate is enabled, false otherwise.
     */
    public boolean isTranslateEnabled() {
        return PrefServiceBridgeJni.get().getTranslateEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether translate is configured by policy
     */
    public boolean isTranslateManaged() {
        return PrefServiceBridgeJni.get().getTranslateManaged(PrefServiceBridge.this);
    }

    /**
     * Sets whether the web service to resolve navigation error should be enabled.
     */
    public void setResolveNavigationErrorEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setResolveNavigationErrorEnabled(
                PrefServiceBridge.this, enabled);
    }

    /**
     * Checks the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @return The state of the corresponding deletion preference.
     */
    public boolean getBrowsingDataDeletionPreference(int dataType, int clearBrowsingDataTab) {
        return PrefServiceBridgeJni.get().getBrowsingDataDeletionPreference(
                PrefServiceBridge.this, dataType, clearBrowsingDataTab);
    }

    /**
     * Sets the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @param value The state to be set.
     */
    public void setBrowsingDataDeletionPreference(
            int dataType, int clearBrowsingDataTab, boolean value) {
        PrefServiceBridgeJni.get().setBrowsingDataDeletionPreference(
                PrefServiceBridge.this, dataType, clearBrowsingDataTab, value);
    }

    /**
     * Gets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @return The currently selected browsing data deletion time period.
     */
    public @TimePeriod int getBrowsingDataDeletionTimePeriod(int clearBrowsingDataTab) {
        return PrefServiceBridgeJni.get().getBrowsingDataDeletionTimePeriod(
                PrefServiceBridge.this, clearBrowsingDataTab);
    }

    /**
     * Sets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @param timePeriod The selected browsing data deletion time period.
     */
    public void setBrowsingDataDeletionTimePeriod(
            int clearBrowsingDataTab, @TimePeriod int timePeriod) {
        PrefServiceBridgeJni.get().setBrowsingDataDeletionTimePeriod(
                PrefServiceBridge.this, clearBrowsingDataTab, timePeriod);
    }

    /**
     * @return The index of the tab last visited by the user in the CBD dialog.
     *         Index 0 is for the basic tab, 1 is the advanced tab.
     */
    public int getLastSelectedClearBrowsingDataTab() {
        return PrefServiceBridgeJni.get().getLastClearBrowsingDataTab(PrefServiceBridge.this);
    }

    /**
     * Set the index of the tab last visited by the user.
     * @param tabIndex The last visited tab index, 0 for basic, 1 for advanced.
     */
    public void setLastSelectedClearBrowsingDataTab(int tabIndex) {
        PrefServiceBridgeJni.get().setLastClearBrowsingDataTab(PrefServiceBridge.this, tabIndex);
    }

    public void setBlockThirdPartyCookiesEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setBlockThirdPartyCookiesEnabled(
                PrefServiceBridge.this, enabled);
    }

    public void setDoNotTrackEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setDoNotTrackEnabled(PrefServiceBridge.this, enabled);
    }

    public void setRememberPasswordsEnabled(boolean allow) {
        PrefServiceBridgeJni.get().setRememberPasswordsEnabled(PrefServiceBridge.this, allow);
    }

    public void setPasswordManagerAutoSigninEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setPasswordManagerAutoSigninEnabled(
                PrefServiceBridge.this, enabled);
    }

    public void setPasswordLeakDetectionEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setPasswordLeakDetectionEnabled(PrefServiceBridge.this, enabled);
    }

    public void setNotificationsVibrateEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setNotificationsVibrateEnabled(PrefServiceBridge.this, enabled);
    }

    public void setPasswordEchoEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setPasswordEchoEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public boolean isPopupsManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public boolean requiresTriStateContentSetting(int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /**
     * Sets the preferences on whether to enable/disable given setting.
     */
    public void setCategoryEnabled(int contentSettingsType, boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BLUETOOTH_SCANNING:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD:
                setContentSettingEnabled(contentSettingsType, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
                PrefServiceBridgeJni.get().setAutomaticDownloadsEnabled(
                        PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY:
                PrefServiceBridgeJni.get().setAutoplayEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
                PrefServiceBridgeJni.get().setBackgroundSyncEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
                PrefServiceBridgeJni.get().setClipboardEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES:
                PrefServiceBridgeJni.get().setAllowCookiesEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                PrefServiceBridgeJni.get().setAllowLocationEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                PrefServiceBridgeJni.get().setCameraEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                PrefServiceBridgeJni.get().setMicEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                PrefServiceBridgeJni.get().setNotificationsEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS:
                PrefServiceBridgeJni.get().setSensorsEnabled(PrefServiceBridge.this, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND:
                PrefServiceBridgeJni.get().setSoundEnabled(PrefServiceBridge.this, allow);
                break;
            default:
                assert false;
        }
    }

    public boolean isCategoryEnabled(int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
            // Returns true if JavaScript is enabled. It may return the temporary value set by
            // {@link #setJavaScriptEnabled}. The default is true.
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
            // Returns true if websites are allowed to request permission to access USB devices.
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BLUETOOTH_SCANNING:
                return isContentSettingEnabled(contentSettingsType);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
                return PrefServiceBridgeJni.get().getAutomaticDownloadsEnabled(
                        PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY:
                return PrefServiceBridgeJni.get().getAutoplayEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
                return PrefServiceBridgeJni.get().getBackgroundSyncEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES:
                return PrefServiceBridgeJni.get().getAcceptCookiesEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                return PrefServiceBridgeJni.get().getCameraEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                return PrefServiceBridgeJni.get().getMicEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                return PrefServiceBridgeJni.get().getNotificationsEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS:
                return PrefServiceBridgeJni.get().getSensorsEnabled(PrefServiceBridge.this);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND:
                return PrefServiceBridgeJni.get().getSoundEnabled(PrefServiceBridge.this);
            default:
                assert false;
                return false;
        }
    }

    /**
     * Gets the ContentSetting for a settings type. Should only be used for more
     * complex settings where a binary on/off value is not sufficient.
     * Otherwise, use isCategoryEnabled() above.
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public int getContentSetting(int contentSettingsType) {
        return PrefServiceBridgeJni.get().getContentSetting(
                PrefServiceBridge.this, contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public void setContentSetting(int contentSettingsType, int setting) {
        PrefServiceBridgeJni.get().setContentSetting(
                PrefServiceBridge.this, contentSettingsType, setting);
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public boolean isCameraManagedByCustodian() {
        return PrefServiceBridgeJni.get().getCameraManagedByCustodian(PrefServiceBridge.this);
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public boolean isCameraUserModifiable() {
        return PrefServiceBridgeJni.get().getCameraUserModifiable(PrefServiceBridge.this);
    }

    /**
     * Sets the preferences on whether to enable/disable microphone.
     */
    public void setMicEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setMicEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether the microphone permission is managed by the custodian of
     * the supervised account.
     */
    public boolean isMicManagedByCustodian() {
        return PrefServiceBridgeJni.get().getMicManagedByCustodian(PrefServiceBridge.this);
    }

    /**
     * @return Whether the microphone permission is editable by the user.
     */
    public boolean isMicUserModifiable() {
        return PrefServiceBridgeJni.get().getMicUserModifiable(PrefServiceBridge.this);
    }

    /**
     * @return true if incognito mode is enabled.
     */
    public boolean isIncognitoModeEnabled() {
        return PrefServiceBridgeJni.get().getIncognitoModeEnabled(PrefServiceBridge.this);
    }

    /**
     * @return true if incognito mode is managed by policy.
     */
    public boolean isIncognitoModeManaged() {
        return PrefServiceBridgeJni.get().getIncognitoModeManaged(PrefServiceBridge.this);
    }

    /**
     * @return Whether printing is enabled.
     */
    public boolean isPrintingEnabled() {
        return PrefServiceBridgeJni.get().getPrintingEnabled(PrefServiceBridge.this);
    }

    /**
     * @return Whether printing is managed by policy.
     */
    public boolean isPrintingManaged() {
        return PrefServiceBridgeJni.get().getPrintingManaged(PrefServiceBridge.this);
    }

    /**
     * Get all the version strings from native.
     * @return AboutVersionStrings about version strings.
     */
    public AboutVersionStrings getAboutVersionStrings() {
        return PrefServiceBridgeJni.get().getAboutVersionStrings(PrefServiceBridge.this);
    }

    /**
     * Reset accept-languages to its default value.
     *
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public void resetAcceptLanguages(String defaultLocale) {
        PrefServiceBridgeJni.get().resetAcceptLanguages(PrefServiceBridge.this, defaultLocale);
    }

    /**
     * @return Whether SafeSites for supervised users is enabled.
     */
    public boolean isSupervisedUserSafeSitesEnabled() {
        return PrefServiceBridgeJni.get().getSupervisedUserSafeSitesEnabled(PrefServiceBridge.this);
    }

    /**
     * @return the default supervised user filtering behavior
     */
    public int getDefaultSupervisedUserFilteringBehavior() {
        return PrefServiceBridgeJni.get().getDefaultSupervisedUserFilteringBehavior(
                PrefServiceBridge.this);
    }

    public String getSupervisedUserCustodianName() {
        return PrefServiceBridgeJni.get().getSupervisedUserCustodianName(PrefServiceBridge.this);
    }

    public String getSupervisedUserCustodianEmail() {
        return PrefServiceBridgeJni.get().getSupervisedUserCustodianEmail(PrefServiceBridge.this);
    }

    public String getSupervisedUserCustodianProfileImageURL() {
        return PrefServiceBridgeJni.get().getSupervisedUserCustodianProfileImageURL(
                PrefServiceBridge.this);
    }

    public String getSupervisedUserSecondCustodianName() {
        return PrefServiceBridgeJni.get().getSupervisedUserSecondCustodianName(
                PrefServiceBridge.this);
    }

    public String getSupervisedUserSecondCustodianEmail() {
        return PrefServiceBridgeJni.get().getSupervisedUserSecondCustodianEmail(
                PrefServiceBridge.this);
    }

    public String getSupervisedUserSecondCustodianProfileImageURL() {
        return PrefServiceBridgeJni.get().getSupervisedUserSecondCustodianProfileImageURL(
                PrefServiceBridge.this);
    }

    /**
     * @return A sorted list of LanguageItems representing the Chrome accept languages with details.
     *         Languages that are not supported on Android have been filtered out.
     */
    public List<LanguageItem> getChromeLanguageList() {
        List<LanguageItem> list = new ArrayList<>();
        PrefServiceBridgeJni.get().getChromeAcceptLanguages(PrefServiceBridge.this, list);
        return list;
    }

    /**
     * @return A sorted list of accept language codes for the current user.
     *         Note that for the signed-in user, the list might contain some language codes from
     *         other platforms but not supported on Android.
     */
    public List<String> getUserLanguageCodes() {
        List<String> list = new ArrayList<>();
        PrefServiceBridgeJni.get().getUserAcceptLanguages(PrefServiceBridge.this, list);
        return list;
    }

    /**
     * Update accept language for the current user.
     *
     * @param languageCode A valid language code to update.
     * @param add Whether this is an "add" operation or "delete" operation.
     */
    public void updateUserAcceptLanguages(String languageCode, boolean add) {
        PrefServiceBridgeJni.get().updateUserAcceptLanguages(
                PrefServiceBridge.this, languageCode, add);
    }

    /**
     * Move a language to the given postion of the user's accept language.
     *
     * @param languageCode A valid language code to set.
     * @param offset The offset from the original position of the language.
     */
    public void moveAcceptLanguage(String languageCode, int offset) {
        PrefServiceBridgeJni.get().moveAcceptLanguage(PrefServiceBridge.this, languageCode, offset);
    }

    /**
     * Given an array of language codes, sets the order of the user's accepted languages to match.
     *
     * @param codes The new order for the user's accepted languages.
     */
    public void setLanguageOrder(String[] codes) {
        PrefServiceBridgeJni.get().setLanguageOrder(PrefServiceBridge.this, codes);
    }

    /**
     * @param languageCode A valid language code to check.
     * @return Whether the given language is blocked by the user.
     */
    public boolean isBlockedLanguage(String languageCode) {
        return PrefServiceBridgeJni.get().isBlockedLanguage(PrefServiceBridge.this, languageCode);
    }

    /**
     * Sets the blocked state of a given language.
     *
     * @param languageCode A valid language code to change.
     * @param blocked Whether to set language blocked.
     */
    public void setLanguageBlockedState(String languageCode, boolean blocked) {
        PrefServiceBridgeJni.get().setLanguageBlockedState(
                PrefServiceBridge.this, languageCode, blocked);
    }


    /**
      * @return Whether usage and crash reporting pref is enabled.
      */
    public boolean isMetricsReportingEnabled() {
        return PrefServiceBridgeJni.get().isMetricsReportingEnabled(PrefServiceBridge.this);
    }

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    public void setMetricsReportingEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setMetricsReportingEnabled(PrefServiceBridge.this, enabled);
    }

    /**
     * @return Whether usage and crash report pref is managed.
     */
    public boolean isMetricsReportingManaged() {
        return PrefServiceBridgeJni.get().isMetricsReportingManaged(PrefServiceBridge.this);
    }

    /**
     * @param clicked Whether the update menu item was clicked. The preference is stored to
     *                facilitate logging whether Chrome was updated after a click on the menu item.
     */
    public void setClickedUpdateMenuItem(boolean clicked) {
        PrefServiceBridgeJni.get().setClickedUpdateMenuItem(PrefServiceBridge.this, clicked);
    }

    /**
     * @return Whether the update menu item was clicked.
     */
    public boolean getClickedUpdateMenuItem() {
        return PrefServiceBridgeJni.get().getClickedUpdateMenuItem(PrefServiceBridge.this);
    }

    /**
     * @param version The latest version of Chrome available when the update menu item
     *                was clicked.
     */
    public void setLatestVersionWhenClickedUpdateMenuItem(String version) {
        PrefServiceBridgeJni.get().setLatestVersionWhenClickedUpdateMenuItem(
                PrefServiceBridge.this, version);
    }

    /**
     * @return The latest version of Chrome available when the update menu item was clicked.
     */
    public String getLatestVersionWhenClickedUpdateMenuItem() {
        return PrefServiceBridgeJni.get().getLatestVersionWhenClickedUpdateMenuItem(
                PrefServiceBridge.this);
    }

    @VisibleForTesting
    public void setSupervisedUserId(String supervisedUserId) {
        PrefServiceBridgeJni.get().setSupervisedUserId(PrefServiceBridge.this, supervisedUserId);
    }

    /**
     * @return The stored download default directory.
     */
    public String getDownloadDefaultDirectory() {
        return PrefServiceBridgeJni.get().getDownloadDefaultDirectory(PrefServiceBridge.this);
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public void setDownloadAndSaveFileDefaultDirectory(String directory) {
        PrefServiceBridgeJni.get().setDownloadAndSaveFileDefaultDirectory(
                PrefServiceBridge.this, directory);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public int getPromptForDownloadAndroid() {
        return PrefServiceBridgeJni.get().getPromptForDownloadAndroid(PrefServiceBridge.this);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        PrefServiceBridgeJni.get().setPromptForDownloadAndroid(PrefServiceBridge.this, status);
    }

    /**
     * @return Whether the explicit language prompt was shown at least once.
     */
    public boolean getExplicitLanguageAskPromptShown() {
        return PrefServiceBridgeJni.get().getExplicitLanguageAskPromptShown(PrefServiceBridge.this);
    }

    /**
     * @param shown The value to set the underlying pref to: whether the prompt
     * was shown to the user at least once.
     */
    public void setExplicitLanguageAskPromptShown(boolean shown) {
        PrefServiceBridgeJni.get().setExplicitLanguageAskPromptShown(PrefServiceBridge.this, shown);
    }

    /**
     * @param enabled The value to set whether or not ForceWebContentsDarkMode is enabled.
     */
    public void setForceWebContentsDarkModeEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setForceWebContentsDarkModeEnabled(
                PrefServiceBridge.this, enabled);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(@Nullable PrefServiceBridge instanceForTesting) {
        sInstance = instanceForTesting;
    }

    @NativeMethods
    public interface Natives {
        boolean isContentSettingEnabled(PrefServiceBridge caller, int contentSettingType);
        boolean isContentSettingManaged(PrefServiceBridge caller, int contentSettingType);
        void setContentSettingEnabled(
                PrefServiceBridge caller, int contentSettingType, boolean allow);
        void getContentSettingsExceptions(PrefServiceBridge caller, int contentSettingsType,
                List<ContentSettingException> list);
        void setContentSettingForPattern(
                PrefServiceBridge caller, int contentSettingType, String pattern, int setting);
        int getContentSetting(PrefServiceBridge caller, int contentSettingType);
        void setContentSetting(PrefServiceBridge caller, int contentSettingType, int setting);
        boolean getBoolean(PrefServiceBridge caller, int preference);
        void setBoolean(PrefServiceBridge caller, int preference, boolean value);
        boolean getAcceptCookiesEnabled(PrefServiceBridge caller);
        boolean getAcceptCookiesUserModifiable(PrefServiceBridge caller);
        boolean getAcceptCookiesManagedByCustodian(PrefServiceBridge caller);
        boolean getAutomaticDownloadsEnabled(PrefServiceBridge caller);
        boolean getAutoplayEnabled(PrefServiceBridge caller);
        boolean getBackgroundSyncEnabled(PrefServiceBridge caller);
        boolean getBlockThirdPartyCookiesEnabled(PrefServiceBridge caller);
        boolean getBlockThirdPartyCookiesManaged(PrefServiceBridge caller);
        boolean getRememberPasswordsEnabled(PrefServiceBridge caller);
        boolean getPasswordManagerAutoSigninEnabled(PrefServiceBridge caller);
        boolean getPasswordLeakDetectionEnabled(PrefServiceBridge caller);
        boolean getRememberPasswordsManaged(PrefServiceBridge caller);
        boolean getPasswordManagerAutoSigninManaged(PrefServiceBridge caller);
        boolean getPasswordLeakDetectionManaged(PrefServiceBridge caller);
        boolean getAllowLocationUserModifiable(PrefServiceBridge caller);
        boolean getLocationAllowedByPolicy(PrefServiceBridge caller);
        boolean getAllowLocationManagedByCustodian(PrefServiceBridge caller);
        boolean getDoNotTrackEnabled(PrefServiceBridge caller);
        boolean getPasswordEchoEnabled(PrefServiceBridge caller);
        boolean getFirstRunEulaAccepted(PrefServiceBridge caller);
        boolean getCameraEnabled(PrefServiceBridge caller);
        void setCameraEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getCameraUserModifiable(PrefServiceBridge caller);
        boolean getCameraManagedByCustodian(PrefServiceBridge caller);
        boolean getMicEnabled(PrefServiceBridge caller);
        void setMicEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getMicUserModifiable(PrefServiceBridge caller);
        boolean getMicManagedByCustodian(PrefServiceBridge caller);
        boolean getTranslateEnabled(PrefServiceBridge caller);
        boolean getTranslateManaged(PrefServiceBridge caller);
        boolean getResolveNavigationErrorEnabled(PrefServiceBridge caller);
        boolean getResolveNavigationErrorManaged(PrefServiceBridge caller);
        boolean getIncognitoModeEnabled(PrefServiceBridge caller);
        boolean getIncognitoModeManaged(PrefServiceBridge caller);
        boolean getPrintingEnabled(PrefServiceBridge caller);
        boolean getPrintingManaged(PrefServiceBridge caller);
        boolean getSensorsEnabled(PrefServiceBridge caller);
        boolean getSoundEnabled(PrefServiceBridge caller);
        boolean getSupervisedUserSafeSitesEnabled(PrefServiceBridge caller);
        void setTranslateEnabled(PrefServiceBridge caller, boolean enabled);
        void migrateJavascriptPreference(PrefServiceBridge caller);
        boolean getBrowsingDataDeletionPreference(
                PrefServiceBridge caller, int dataType, int clearBrowsingDataTab);
        void setBrowsingDataDeletionPreference(
                PrefServiceBridge caller, int dataType, int clearBrowsingDataTab, boolean value);
        int getBrowsingDataDeletionTimePeriod(PrefServiceBridge caller, int clearBrowsingDataTab);
        void setBrowsingDataDeletionTimePeriod(
                PrefServiceBridge caller, int clearBrowsingDataTab, int timePeriod);
        int getLastClearBrowsingDataTab(PrefServiceBridge caller);
        void setLastClearBrowsingDataTab(PrefServiceBridge caller, int lastTab);
        void setAutomaticDownloadsEnabled(PrefServiceBridge caller, boolean enabled);
        void setAutoplayEnabled(PrefServiceBridge caller, boolean enabled);
        void setAllowCookiesEnabled(PrefServiceBridge caller, boolean enabled);
        void setBackgroundSyncEnabled(PrefServiceBridge caller, boolean enabled);
        void setBlockThirdPartyCookiesEnabled(PrefServiceBridge caller, boolean enabled);
        void setClipboardEnabled(PrefServiceBridge caller, boolean enabled);
        void setDoNotTrackEnabled(PrefServiceBridge caller, boolean enabled);
        void setRememberPasswordsEnabled(PrefServiceBridge caller, boolean allow);
        void setPasswordManagerAutoSigninEnabled(PrefServiceBridge caller, boolean enabled);
        void setPasswordLeakDetectionEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getAllowLocationEnabled(PrefServiceBridge caller);
        boolean getNotificationsEnabled(PrefServiceBridge caller);
        boolean getNotificationsVibrateEnabled(PrefServiceBridge caller);
        void setAllowLocationEnabled(PrefServiceBridge caller, boolean enabled);
        void setNotificationsEnabled(PrefServiceBridge caller, boolean enabled);
        void setNotificationsVibrateEnabled(PrefServiceBridge caller, boolean enabled);
        void setPasswordEchoEnabled(PrefServiceBridge caller, boolean enabled);
        void setSensorsEnabled(PrefServiceBridge caller, boolean enabled);
        void setSoundEnabled(PrefServiceBridge caller, boolean enabled);
        boolean canPrefetchAndPrerender(PrefServiceBridge caller);
        AboutVersionStrings getAboutVersionStrings(PrefServiceBridge caller);
        void setContextualSearchPreference(PrefServiceBridge caller, String preference);
        String getContextualSearchPreference(PrefServiceBridge caller);
        boolean getContextualSearchPreferenceIsManaged(PrefServiceBridge caller);
        boolean getSearchSuggestEnabled(PrefServiceBridge caller);
        void setSearchSuggestEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getSearchSuggestManaged(PrefServiceBridge caller);
        boolean getSafeBrowsingExtendedReportingEnabled(PrefServiceBridge caller);
        void setSafeBrowsingExtendedReportingEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getSafeBrowsingExtendedReportingManaged(PrefServiceBridge caller);
        boolean getSafeBrowsingEnabled(PrefServiceBridge caller);
        void setSafeBrowsingEnabled(PrefServiceBridge caller, boolean enabled);
        boolean getSafeBrowsingManaged(PrefServiceBridge caller);
        boolean getNetworkPredictionManaged(PrefServiceBridge caller);
        boolean obsoleteNetworkPredictionOptionsHasUserSetting(PrefServiceBridge caller);
        boolean getNetworkPredictionEnabled(PrefServiceBridge caller);
        void setNetworkPredictionEnabled(PrefServiceBridge caller, boolean enabled);
        void setResolveNavigationErrorEnabled(PrefServiceBridge caller, boolean enabled);
        void setEulaAccepted(PrefServiceBridge caller);
        void resetAcceptLanguages(PrefServiceBridge caller, String defaultLocale);
        String getSyncLastAccountId(PrefServiceBridge caller);
        String getSyncLastAccountName(PrefServiceBridge caller);
        String getSupervisedUserCustodianName(PrefServiceBridge caller);
        String getSupervisedUserCustodianEmail(PrefServiceBridge caller);
        String getSupervisedUserCustodianProfileImageURL(PrefServiceBridge caller);
        int getDefaultSupervisedUserFilteringBehavior(PrefServiceBridge caller);
        String getSupervisedUserSecondCustodianName(PrefServiceBridge caller);
        String getSupervisedUserSecondCustodianEmail(PrefServiceBridge caller);
        String getSupervisedUserSecondCustodianProfileImageURL(PrefServiceBridge caller);
        boolean isMetricsReportingEnabled(PrefServiceBridge caller);
        void setMetricsReportingEnabled(PrefServiceBridge caller, boolean enabled);
        boolean isMetricsReportingManaged(PrefServiceBridge caller);
        void setClickedUpdateMenuItem(PrefServiceBridge caller, boolean clicked);
        boolean getClickedUpdateMenuItem(PrefServiceBridge caller);
        void setLatestVersionWhenClickedUpdateMenuItem(PrefServiceBridge caller, String version);
        String getLatestVersionWhenClickedUpdateMenuItem(PrefServiceBridge caller);
        void setSupervisedUserId(PrefServiceBridge caller, String supervisedUserId);
        void getChromeAcceptLanguages(PrefServiceBridge caller, List<LanguageItem> list);
        void getUserAcceptLanguages(PrefServiceBridge caller, List<String> list);
        void updateUserAcceptLanguages(PrefServiceBridge caller, String language, boolean add);
        void moveAcceptLanguage(PrefServiceBridge caller, String language, int offset);
        void setLanguageOrder(PrefServiceBridge caller, String[] codes);
        boolean isBlockedLanguage(PrefServiceBridge caller, String language);
        void setLanguageBlockedState(PrefServiceBridge caller, String language, boolean blocked);
        String getDownloadDefaultDirectory(PrefServiceBridge caller);
        void setDownloadAndSaveFileDefaultDirectory(PrefServiceBridge caller, String directory);
        int getPromptForDownloadAndroid(PrefServiceBridge caller);
        void setPromptForDownloadAndroid(PrefServiceBridge caller, int status);
        boolean getExplicitLanguageAskPromptShown(PrefServiceBridge caller);
        void setExplicitLanguageAskPromptShown(PrefServiceBridge caller, boolean shown);
        void setForceWebContentsDarkModeEnabled(PrefServiceBridge caller, boolean enabled);
    }
}

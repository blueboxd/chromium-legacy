// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ContentSettingsType;
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
        return PrefServiceBridgeJni.get().getBoolean(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setBoolean(@Pref int preference, boolean value) {
        PrefServiceBridgeJni.get().setBoolean(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    public int getInteger(@Pref int preference) {
        return PrefServiceBridgeJni.get().getInteger(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setInteger(@Pref int preference, int value) {
        PrefServiceBridgeJni.get().setInteger(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    @NonNull
    public String getString(@Pref int preference) {
        return PrefServiceBridgeJni.get().getString(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setString(@Pref int preference, @NonNull String value) {
        PrefServiceBridgeJni.get().setString(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is managed.
     */
    public boolean isManagedPreference(@Pref int preference) {
        return PrefServiceBridgeJni.get().isManagedPreference(preference);
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingEnabled(int contentSettingsType) {
        return PrefServiceBridgeJni.get().isContentSettingEnabled(contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingManaged(int contentSettingsType) {
        return PrefServiceBridgeJni.get().isContentSettingManaged(contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public void setContentSettingEnabled(int contentSettingsType, boolean enabled) {
        PrefServiceBridgeJni.get().setContentSettingEnabled(contentSettingsType, enabled);
    }

    /**
     * Returns all the currently saved exceptions for a given content settings type.
     * @param contentSettingsType The type to fetch exceptions for.
     */
    public List<ContentSettingException> getContentSettingsExceptions(int contentSettingsType) {
        List<ContentSettingException> list = new ArrayList<ContentSettingException>();
        PrefServiceBridgeJni.get().getContentSettingsExceptions(contentSettingsType, list);
        return list;
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(ArrayList<ContentSettingException> list,
            int contentSettingsType, String pattern, int contentSetting, String source) {
        ContentSettingException exception =
                new ContentSettingException(contentSettingsType, pattern, contentSetting, source);
        list.add(exception);
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
            case ContentSettingsType.GEOLOCATION:
                return Arrays.copyOf(LOCATION_PERMISSIONS, LOCATION_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    /**
     * @return Whether cookies acceptance is modifiable by the user
     */
    public boolean isAcceptCookiesUserModifiable() {
        return PrefServiceBridgeJni.get().getAcceptCookiesUserModifiable();
    }

    /**
     * @return Whether cookies acceptance is configured by the user's custodian
     * (for supervised users).
     */
    public boolean isAcceptCookiesManagedByCustodian() {
        return PrefServiceBridgeJni.get().getAcceptCookiesManagedByCustodian();
    }

    /**
     * @return Whether geolocation information can be shared with content.
     */
    public boolean isAllowLocationEnabled() {
        return PrefServiceBridgeJni.get().getAllowLocationEnabled();
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public boolean isLocationAllowedByPolicy() {
        return PrefServiceBridgeJni.get().getLocationAllowedByPolicy();
    }

    /**
     * @return Whether the location preference is modifiable by the user.
     */
    public boolean isAllowLocationUserModifiable() {
        return PrefServiceBridgeJni.get().getAllowLocationUserModifiable();
    }

    /**
     * @return Whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public boolean isAllowLocationManagedByCustodian() {
        return PrefServiceBridgeJni.get().getAllowLocationManagedByCustodian();
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public boolean isFirstRunEulaAccepted() {
        return PrefServiceBridgeJni.get().getFirstRunEulaAccepted();
    }

    /**
     * @return Whether JavaScript is managed by policy.
     */
    public boolean javaScriptManaged() {
        return isContentSettingManaged(ContentSettingsType.JAVASCRIPT);
    }

    /**
     * @return true if background sync is managed by policy.
     */
    public boolean isBackgroundSyncManaged() {
        return isContentSettingManaged(ContentSettingsType.BACKGROUND_SYNC);
    }

    /**
     * @return true if automatic downloads is managed by policy.
     */
    public boolean isAutomaticDownloadsManaged() {
        return isContentSettingManaged(ContentSettingsType.AUTOMATIC_DOWNLOADS);
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
    public void setEulaAccepted() {
        PrefServiceBridgeJni.get().setEulaAccepted();
    }

    /**
     * @return Whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration. See http://crbug.com/334602
     */
    public boolean obsoleteNetworkPredictionOptionsHasUserSetting() {
        return PrefServiceBridgeJni.get().obsoleteNetworkPredictionOptionsHasUserSetting();
    }

    /**
     * @return Network predictions preference.
     */
    public boolean getNetworkPredictionEnabled() {
        return PrefServiceBridgeJni.get().getNetworkPredictionEnabled();
    }

    /**
     * Sets network predictions preference.
     */
    public void setNetworkPredictionEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setNetworkPredictionEnabled(enabled);
    }

    /**
     * @return Whether Network Predictions is configured by policy.
     */
    public boolean isNetworkPredictionManaged() {
        return PrefServiceBridgeJni.get().getNetworkPredictionManaged();
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    public boolean canPrefetchAndPrerender() {
        return PrefServiceBridgeJni.get().canPrefetchAndPrerender();
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public boolean isPopupsManaged() {
        return isContentSettingManaged(ContentSettingsType.POPUPS);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public boolean requiresTriStateContentSetting(int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
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
            case ContentSettingsType.ADS:
            case ContentSettingsType.BLUETOOTH_SCANNING:
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
            case ContentSettingsType.USB_GUARD:
                setContentSettingEnabled(contentSettingsType, allow);
                break;
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                PrefServiceBridgeJni.get().setAutomaticDownloadsEnabled(allow);
                break;
            case ContentSettingsType.AUTOPLAY:
                PrefServiceBridgeJni.get().setAutoplayEnabled(allow);
                break;
            case ContentSettingsType.BACKGROUND_SYNC:
                PrefServiceBridgeJni.get().setBackgroundSyncEnabled(allow);
                break;
            case ContentSettingsType.CLIPBOARD_READ:
                PrefServiceBridgeJni.get().setClipboardEnabled(allow);
                break;
            case ContentSettingsType.COOKIES:
                PrefServiceBridgeJni.get().setAllowCookiesEnabled(allow);
                break;
            case ContentSettingsType.GEOLOCATION:
                PrefServiceBridgeJni.get().setAllowLocationEnabled(allow);
                break;
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                PrefServiceBridgeJni.get().setCameraEnabled(allow);
                break;
            case ContentSettingsType.MEDIASTREAM_MIC:
                PrefServiceBridgeJni.get().setMicEnabled(allow);
                break;
            case ContentSettingsType.NOTIFICATIONS:
                PrefServiceBridgeJni.get().setNotificationsEnabled(allow);
                break;
            case ContentSettingsType.SENSORS:
                PrefServiceBridgeJni.get().setSensorsEnabled(allow);
                break;
            case ContentSettingsType.SOUND:
                PrefServiceBridgeJni.get().setSoundEnabled(allow);
                break;
            default:
                assert false;
        }
    }

    public boolean isCategoryEnabled(int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.CLIPBOARD_READ:
            // Returns true if JavaScript is enabled. It may return the temporary value set by
            // {@link #setJavaScriptEnabled}. The default is true.
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
            // Returns true if websites are allowed to request permission to access USB devices.
            case ContentSettingsType.USB_GUARD:
            case ContentSettingsType.BLUETOOTH_SCANNING:
                return isContentSettingEnabled(contentSettingsType);
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return PrefServiceBridgeJni.get().getAutomaticDownloadsEnabled();
            case ContentSettingsType.AUTOPLAY:
                return PrefServiceBridgeJni.get().getAutoplayEnabled();
            case ContentSettingsType.BACKGROUND_SYNC:
                return PrefServiceBridgeJni.get().getBackgroundSyncEnabled();
            case ContentSettingsType.COOKIES:
                return PrefServiceBridgeJni.get().getAcceptCookiesEnabled();
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return PrefServiceBridgeJni.get().getCameraEnabled();
            case ContentSettingsType.MEDIASTREAM_MIC:
                return PrefServiceBridgeJni.get().getMicEnabled();
            case ContentSettingsType.NOTIFICATIONS:
                return PrefServiceBridgeJni.get().getNotificationsEnabled();
            case ContentSettingsType.SENSORS:
                return PrefServiceBridgeJni.get().getSensorsEnabled();
            case ContentSettingsType.SOUND:
                return PrefServiceBridgeJni.get().getSoundEnabled();
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
        return PrefServiceBridgeJni.get().getContentSetting(contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public void setContentSetting(int contentSettingsType, int setting) {
        PrefServiceBridgeJni.get().setContentSetting(contentSettingsType, setting);
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public boolean isCameraManagedByCustodian() {
        return PrefServiceBridgeJni.get().getCameraManagedByCustodian();
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public boolean isCameraUserModifiable() {
        return PrefServiceBridgeJni.get().getCameraUserModifiable();
    }

    /**
     * @return Whether the microphone permission is managed by the custodian of
     * the supervised account.
     */
    public boolean isMicManagedByCustodian() {
        return PrefServiceBridgeJni.get().getMicManagedByCustodian();
    }

    /**
     * @return Whether the microphone permission is editable by the user.
     */
    public boolean isMicUserModifiable() {
        return PrefServiceBridgeJni.get().getMicUserModifiable();
    }

    /**
     * @return true if incognito mode is enabled.
     */
    public boolean isIncognitoModeEnabled() {
        return PrefServiceBridgeJni.get().getIncognitoModeEnabled();
    }

    /**
     * @return true if incognito mode is managed by policy.
     */
    public boolean isIncognitoModeManaged() {
        return PrefServiceBridgeJni.get().getIncognitoModeManaged();
    }

    /**
      * @return Whether usage and crash reporting pref is enabled.
      */
    public boolean isMetricsReportingEnabled() {
        return PrefServiceBridgeJni.get().isMetricsReportingEnabled();
    }

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    public void setMetricsReportingEnabled(boolean enabled) {
        PrefServiceBridgeJni.get().setMetricsReportingEnabled(enabled);
    }

    /**
     * @return Whether usage and crash report pref is managed.
     */
    public boolean isMetricsReportingManaged() {
        return PrefServiceBridgeJni.get().isMetricsReportingManaged();
    }

    public void setContentSettingForPattern(int contentSettingType, String pattern, int setting) {
        PrefServiceBridgeJni.get().setContentSettingForPattern(
                contentSettingType, pattern, setting);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(@Nullable PrefServiceBridge instanceForTesting) {
        sInstance = instanceForTesting;
    }

    @NativeMethods
    public interface Natives {
        boolean isContentSettingEnabled(int contentSettingType);
        boolean isContentSettingManaged(int contentSettingType);
        void setContentSettingEnabled(int contentSettingType, boolean allow);
        void getContentSettingsExceptions(
                int contentSettingsType, List<ContentSettingException> list);
        void setContentSettingForPattern(int contentSettingType, String pattern, int setting);
        int getContentSetting(int contentSettingType);
        void setContentSetting(int contentSettingType, int setting);
        boolean getBoolean(int preference);
        void setBoolean(int preference, boolean value);
        int getInteger(int preference);
        void setInteger(int preference, int value);
        String getString(int preference);
        void setString(int preference, String value);
        boolean isManagedPreference(int preference);
        boolean getAcceptCookiesEnabled();
        boolean getAcceptCookiesUserModifiable();
        boolean getAcceptCookiesManagedByCustodian();
        boolean getAutomaticDownloadsEnabled();
        boolean getAutoplayEnabled();
        boolean getBackgroundSyncEnabled();
        boolean getAllowLocationUserModifiable();
        boolean getLocationAllowedByPolicy();
        boolean getAllowLocationManagedByCustodian();
        boolean getFirstRunEulaAccepted();
        boolean getCameraEnabled();
        void setCameraEnabled(boolean enabled);
        boolean getCameraUserModifiable();
        boolean getCameraManagedByCustodian();
        boolean getMicEnabled();
        void setMicEnabled(boolean enabled);
        boolean getMicUserModifiable();
        boolean getMicManagedByCustodian();
        boolean getIncognitoModeEnabled();
        boolean getIncognitoModeManaged();
        boolean getSensorsEnabled();
        boolean getSoundEnabled();
        void setAutomaticDownloadsEnabled(boolean enabled);
        void setAutoplayEnabled(boolean enabled);
        void setAllowCookiesEnabled(boolean enabled);
        void setBackgroundSyncEnabled(boolean enabled);
        void setClipboardEnabled(boolean enabled);
        boolean getAllowLocationEnabled();
        boolean getNotificationsEnabled();
        void setAllowLocationEnabled(boolean enabled);
        void setNotificationsEnabled(boolean enabled);
        void setSensorsEnabled(boolean enabled);
        void setSoundEnabled(boolean enabled);
        boolean canPrefetchAndPrerender();
        boolean getNetworkPredictionManaged();
        boolean obsoleteNetworkPredictionOptionsHasUserSetting();
        boolean getNetworkPredictionEnabled();
        void setNetworkPredictionEnabled(boolean enabled);
        void setEulaAccepted();
        boolean isMetricsReportingEnabled();
        void setMetricsReportingEnabled(boolean enabled);
        boolean isMetricsReportingManaged();
    }
}

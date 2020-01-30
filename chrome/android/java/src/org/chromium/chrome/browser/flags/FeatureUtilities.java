// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.speech.RecognizerIntent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A class to cache the state of flags from {@link ChromeFeatureList}.
 *
 * It caches certain feature flags that must take effect on startup before native is initialized.
 * ChromeFeatureList can only be queried through native code. The caching is done in
 * {@link android.content.SharedPreferences}, which is available in Java immediately.
 *
 * To cache a flag from ChromeFeatureList:
 * - Set its default value by adding an entry to {@link #sDefaults}.
 * - Add it to the list passed to {@link #cacheNativeFlags()}.
 * - Call {@link #isEnabled(String)} to query whether the cached flag is enabled.
 *   Consider this the source of truth for whether the flag is turned on in the current session.
 * - When querying whether a cached feature is enabled from native, a @CalledByNative method can be
 *   exposed in this file to allow feature_utilities.cc to retrieve the cached value.
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class FeatureUtilities {
    /**
     * Stores the default values for each feature flag queried, used as a fallback in case native
     * isn't loaded, and no value has been previously cached.
     */
    private static Map<String, Boolean> sDefaults = new HashMap<String, Boolean>() {
        { put(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, false); }
    };

    private static Map<String, Boolean> sFlags = new HashMap<>();
    private static Boolean sHasRecognitionIntentHandler;
    private static String sReachedCodeProfilerTrialGroup;
    private static Boolean sEnabledTabThumbnailApsectRatioForTesting;
    private static final String ALLOW_TO_REFETCH = "allow_to_refetch";

    /**
     * Checks if a cached feature flag is enabled.
     *
     * Requires that the feature be registered in {@link #sDefaults}.
     *
     * Rules from highest to lowest priority:
     * 1. If the flag has been forced by {@link #setForTesting}, the forced value is returned.
     * 2. If a value was previously returned in the same run, the same value is returned for
     *    consistency.
     * 3. If native is loaded, the value from {@link ChromeFeatureList} is returned.
     * 4. If in a previous run, the value from {@link ChromeFeatureList} was cached to SharedPrefs,
     *    it is returned.
     * 5. The default value defined in {@link #sDefaults} is returned.
     *
     * @param featureName the feature name from ChromeFeatureList.
     * @return whether the cached feature should be considered enabled.
     */
    public static boolean isEnabled(String featureName) {
        // All cached feature flags should have a default value.
        if (!sDefaults.containsKey(featureName)) {
            throw new IllegalArgumentException(
                    "Feature " + featureName + " has no default in FeatureUtilities.");
        }

        String preferenceName = getPrefForFeatureFlag(featureName);

        Boolean flag = sFlags.get(preferenceName);
        if (flag != null) {
            return flag;
        }

        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        if (prefs.contains(preferenceName)) {
            flag = prefs.readBoolean(preferenceName, false);
        } else {
            flag = sDefaults.get(featureName);
        }
        sFlags.put(preferenceName, flag);
        return flag;
    }

    /**
     * Caches the value of a feature from {@link ChromeFeatureList} to SharedPrefs.
     *
     * @param featureName the feature name from ChromeFeatureList.
     */
    private static void cacheFeature(String featureName) {
        String preferenceName = getPrefForFeatureFlag(featureName);
        boolean isEnabledInNative = ChromeFeatureList.isEnabled(featureName);
        SharedPreferencesManager.getInstance().writeBoolean(preferenceName, isEnabledInNative);
    }

    /**
     * Forces a feature to be enabled or disabled for testing.
     *
     * @param featureName the feature name from ChromeFeatureList.
     * @param value the value that {@link #isEnabled(String)} will be forced to return. If null,
     *     remove any values previously forced.
     */
    public static void setForTesting(String featureName, @Nullable Boolean value) {
        String preferenceName = getPrefForFeatureFlag(featureName);
        sFlags.put(preferenceName, value);
    }

    /**
     * Determines whether or not the {@link RecognizerIntent#ACTION_WEB_SEARCH} {@link Intent}
     * is handled by any {@link android.app.Activity}s in the system.  The result will be cached for
     * future calls.  Passing {@code false} to {@code useCachedValue} will force it to re-query any
     * {@link android.app.Activity}s that can process the {@link Intent}.
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    public static boolean isRecognitionIntentPresent(boolean useCachedValue) {
        ThreadUtils.assertOnUiThread();
        if (sHasRecognitionIntentHandler == null || !useCachedValue) {
            List<ResolveInfo> activities = PackageManagerUtils.queryIntentActivities(
                    new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH), 0);
            sHasRecognitionIntentHandler = !activities.isEmpty();
        }

        return sHasRecognitionIntentHandler;
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags(List<String> featuresToCache) {
        for (String featureName : featuresToCache) {
            if (!sDefaults.containsKey(featureName)) {
                throw new IllegalArgumentException(
                        "Feature " + featureName + " has no default in FeatureUtilities.");
            }
            cacheFeature(featureName);
        }
    }

    /**
     * Caches a predetermined list of flags that must take effect on startup but are set via native
     * code.
     *
     * Do not add new simple boolean flags here, use {@link #cacheNativeFlags} instead.
     */
    public static void cacheAdditionalNativeFlags() {
        cacheCommandLineOnNonRootedEnabled();
        cacheBottomToolbarEnabled();
        cacheAdaptiveToolbarEnabled();
        cacheLabeledBottomToolbarEnabled();
        cacheNightModeAvailable();
        cacheNightModeDefaultToLight();
        cacheNightModeForCustomTabsAvailable();
        cacheDownloadAutoResumptionEnabledInNative();
        cachePrioritizeBootstrapTasks();
        cacheFeedEnabled();
        cacheNetworkServiceWarmUpEnabled();
        cacheImmersiveUiModeEnabled();
        cacheSwapPixelFormatToFixConvertFromTranslucentEnabled();
        cacheReachedCodeProfilerTrialGroup();
        cacheStartSurfaceEnabled();
        cacheNativeTabSwitcherUiFlags();
        cachePaintPreviewTestEnabled();
        cacheAllowToRefetchTabThumbnail();

        // Propagate REACHED_CODE_PROFILER feature value to LibraryLoader. This can't be done in
        // LibraryLoader itself because it lives in //base and can't depend on ChromeFeatureList.
        LibraryLoader.setReachedCodeProfilerEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * Caches flags that are enabled in ServiceManager only mode and must take effect on startup but
     * are set via native code. This function needs to be called in ServiceManager only mode to mark
     * these field trials as active, otherwise histogram data recorded in ServiceManager only mode
     * won't be tagged with their corresponding field trial experiments.
     */
    public static void cacheNativeFlagsForServiceManagerOnlyMode() {
        // TODO(crbug.com/995355): Move other related flags from {@link cacheNativeFlags} to here.
        cacheServiceManagerForDownloadResumption();
        cacheServiceManagerForBackgroundPrefetch();
    }

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public static boolean isTabModelMergingEnabled() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)) {
            return false;
        }
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M;
    }

    private static void cacheServiceManagerForDownloadResumption() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION,
                ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD);
    }

    /**
     * @return if DownloadResumptionBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForDownloadResumptionEnabled() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION, false);
    }

    public static void cacheServiceManagerForBackgroundPrefetch() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH,
                ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH);
    }

    /**
     * @return if PrefetchBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForBackgroundPrefetchEnabled() {
        return isFlagEnabled(
                       ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH,
                       false)
                && isFeedEnabled();
    }

    /**
     * Cache the value of the flag whether or not to use Feed so it can be checked in Java before
     * native is loaded.
     */
    public static void cacheFeedEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_INTEREST_FEED_CONTENT_SUGGESTIONS,
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS);
    }

    /**
     * @return Whether or not the Feed is enabled (based on the cached value in SharedPrefs).
     */
    public static boolean isFeedEnabled() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_INTEREST_FEED_CONTENT_SUGGESTIONS, false);
    }

    /**
     * @return Whether or not the download auto-resumptions should be enabled in native.
     */
    @CalledByNative
    public static boolean isDownloadAutoResumptionEnabledInNative() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE, true);
    }

    /**
     * Cache whether or not the bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheBottomToolbarEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_BOTTOM_TOOLBAR_ENABLED,
                ChromeFeatureList.CHROME_DUET);
        cacheBottomToolbarVariation();
    }

    /**
     * Cache the enabled bottom toolbar variation.
     */
    public static void cacheBottomToolbarVariation() {
        cacheVariation(ChromePreferenceKeys.VARIATION_CACHED_BOTTOM_TOOLBAR,
                ChromeFeatureList.CHROME_DUET,
                BottomToolbarVariationManager.getVariationParamName());
    }

    /**
     * Cache whether or not the adaptive toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheAdaptiveToolbarEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_ADAPTIVE_TOOLBAR_ENABLED,
                ChromeFeatureList.CHROME_DUET_ADAPTIVE);
    }

    /**
     * Cache whether or not the labeled bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheLabeledBottomToolbarEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_LABELED_BOTTOM_TOOLBAR_ENABLED,
                ChromeFeatureList.CHROME_DUET_LABELED);
    }

    /**
     * Cache whether or not download auto-resumptions are enabled in native so on next startup, the
     * value can be made available immediately.
     */
    private static void cacheDownloadAutoResumptionEnabledInNative() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE,
                ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE);
    }

    @VisibleForTesting
    public static void setDownloadAutoResumptionEnabledInNativeForTesting(Boolean value) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE, value);
    }

    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        // TODO(crbug.com/944228): TabGroupsAndroid and ChromeDuet are incompatible for now.
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_BOTTOM_TOOLBAR_ENABLED, false)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext())
                && (isDuetTabStripIntegrationAndroidEnabled() || !isTabGroupsAndroidEnabled());
    }

    /**
     * @return The currently enabled bottom toolbar variation.
     */
    public static String getBottomToolbarVariation() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.VARIATION_CACHED_BOTTOM_TOOLBAR,
                BottomToolbarVariationManager.Variations.NONE);
    }

    /**
     * Set whether the bottom toolbar is enabled for tests. Reset to null at the end of tests.
     */
    @VisibleForTesting
    public static void setIsBottomToolbarEnabledForTesting(Boolean enabled) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_BOTTOM_TOOLBAR_ENABLED, enabled);
    }

    /**
     * @return Whether or not the adaptive toolbar is enabled.
     */
    public static boolean isAdaptiveToolbarEnabled() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_ADAPTIVE_TOOLBAR_ENABLED, true)
                && isBottomToolbarEnabled();
    }

    /**
     * @return Whether or not the labeled bottom toolbar is enabled.
     */
    public static boolean isLabeledBottomToolbarEnabled() {
        return isFlagEnabled(
                       ChromePreferenceKeys.FLAGS_CACHED_LABELED_BOTTOM_TOOLBAR_ENABLED, false)
                && isBottomToolbarEnabled();
    }

    /**
     * Cache whether or not night mode is available (i.e. night mode experiment is enabled) so on
     * next startup, the value can be made available immediately.
     */
    public static void cacheNightModeAvailable() {
        boolean available = ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE)
                || (BuildInfo.isAtLeastQ()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE_FOR_Q));
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_AVAILABLE, available);
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled).
     */
    public static boolean isNightModeAvailable() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_AVAILABLE, true);
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeAvailableForTesting(@Nullable Boolean available) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_AVAILABLE, available);
    }

    /**
     * Cache whether or not to default to the light theme when the night mode feature is enabled.
     */
    public static void cacheNightModeDefaultToLight() {
        // Do not cache on Q (where defaulting to light theme does not apply) or if night mode is
        // not enabled.
        if (BuildInfo.isAtLeastQ()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE)) {
            return;
        }

        String lightModeDefaultParam = "default_light_theme";
        boolean lightModeAsDefault = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_NIGHT_MODE, lightModeDefaultParam, true);

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_DEFAULT_TO_LIGHT, lightModeAsDefault);
    }

    /**
     * @return Whether or not to default to the light theme when the night mode feature is enabled.
     */
    public static boolean isNightModeDefaultToLight() {
        if (BuildInfo.isAtLeastQ()) {
            return false;
        }
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_DEFAULT_TO_LIGHT, true);
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeDefaultToLightForTesting(@Nullable Boolean available) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_DEFAULT_TO_LIGHT, available);
    }

    /**
     * Cache whether or not night mode is available for custom tabs (i.e. night mode experiment is
     * enabled), so the value is immediately available on next start-up.
     */
    public static void cacheNightModeForCustomTabsAvailable() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_CCT_AVAILABLE,
                ChromeFeatureList.ANDROID_NIGHT_MODE_CCT);
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled) for custom tabs.
     */
    public static boolean isNightModeForCustomTabsAvailable() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_CCT_AVAILABLE, true);
    }

    /**
     * Toggles whether the night mode for custom tabs experiment is enabled. Must only be used for
     * testing. Should be reset back to NULL after the test has finished.
     */
    public static void setNightModeForCustomTabsAvailableForTesting(Boolean available) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_NIGHT_MODE_CCT_AVAILABLE, available);
    }

    /**
     * Cache whether or not command line is enabled on non-rooted devices.
     */
    private static void cacheCommandLineOnNonRootedEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED,
                ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
    }

    public static boolean isCommandLineOnNonRootedEnabled() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED, false);
    }

    /**
     * @return Whether or not the download progress infobar is enabled.
     */
    public static boolean isDownloadProgressInfoBarEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR);
    }

    private static void cacheStartSurfaceEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED,
                ChromeFeatureList.START_SURFACE_ANDROID);
        String feature = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation");
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY,
                feature.equals("single"));
    }

    /**
     * @return Whether the Start Surface is enabled.
     */
    public static boolean isStartSurfaceEnabled() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED, false)
                && !SysUtils.isLowEndDevice();
    }

    private static void cachePaintPreviewTestEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_PAINT_PREVIEW_TEST_ENABLED_KEY,
                ChromeFeatureList.PAINT_PREVIEW_TEST);
    }

    /**
     * @return Whether the Paint Preview tapture test is enabled
     */
    public static boolean isPaintPreviewTestEnabled() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_PAINT_PREVIEW_TEST_ENABLED_KEY, false);
    }

    @VisibleForTesting
    static void cacheNativeTabSwitcherUiFlags() {
        if (isEligibleForTabUiExperiments()) {
            cacheGridTabSwitcherEnabled();
            cacheTabGroupsAndroidEnabled();
            cacheDuetTabStripIntegrationAndroidEnabled();
        }
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        return isStartSurfaceEnabled()
                && isFlagEnabled(ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY, false);
    }

    @VisibleForTesting
    static void cacheGridTabSwitcherEnabled() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        String featureKey = ChromePreferenceKeys.FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED;
        boolean shouldQueryFeatureFlag = !DeviceClassManager.enableAccessibilityLayout();
        if (!shouldQueryFeatureFlag) {
            sharedPreferencesManager.writeBoolean(featureKey, false);
            return;
        }
        boolean queriedFlagValue =
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID);
        boolean gridTabSwitcherEnabled =
                queriedFlagValue && TabManagementModuleProvider.isTabManagementModuleSupported();
        sharedPreferencesManager.writeBoolean(featureKey, gridTabSwitcherEnabled);
    }

    /**
     * @return Whether the Grid Tab Switcher UI is enabled and available for use.
     */
    public static boolean isGridTabSwitcherEnabled() {
        // TODO(yusufo): AccessibilityLayout check should not be here and the flow should support
        // changing that setting while Chrome is alive.
        // Having Tab Groups or Start implies Grid Tab Switcher.
        return !(isTabGroupsAndroidContinuationChromeFlagEnabled() && SysUtils.isLowEndDevice())
                && isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED, false)
                || isTabGroupsAndroidEnabled() || isStartSurfaceEnabled();
    }

    /**
     * Toggles whether the Grid Tab Switcher is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setGridTabSwitcherEnabledForTesting(@Nullable Boolean enabled) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED, enabled);
    }

    @VisibleForTesting
    static void cacheTabGroupsAndroidEnabled() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        String featureKey = ChromePreferenceKeys.FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED;
        boolean shouldQueryFeatureFlag = !DeviceClassManager.enableAccessibilityLayout();
        if (!shouldQueryFeatureFlag) {
            sharedPreferencesManager.writeBoolean(featureKey, false);
            return;
        }
        boolean queriedFlagValue =
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID);
        boolean tabGroupsEnabled =
                queriedFlagValue && TabManagementModuleProvider.isTabManagementModuleSupported();
        sharedPreferencesManager.writeBoolean(featureKey, tabGroupsEnabled);
    }

    private static void cacheDuetTabStripIntegrationAndroidEnabled() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        String featureKey =
                ChromePreferenceKeys.FLAGS_CACHED_DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED;
        boolean shouldQueryFeatureFlag = !DeviceClassManager.enableAccessibilityLayout();
        if (!shouldQueryFeatureFlag) {
            sharedPreferencesManager.writeBoolean(featureKey, false);
            return;
        }
        boolean queriedFlagValue =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID);
        boolean duetTabStripIntegrationEnabled =
                queriedFlagValue && TabManagementModuleProvider.isTabManagementModuleSupported();
        sharedPreferencesManager.writeBoolean(featureKey, duetTabStripIntegrationEnabled);
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED, false);
    }

    /**
     * Toggles whether the Tab Group is enabled for testing. Should be reset back to null after the
     * test has finished.
     */
    @VisibleForTesting
    public static void setTabGroupsAndroidEnabledForTesting(@Nullable Boolean available) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED, available);
    }

    /**
     * Toggles whether the StartSurface is enabled for testing. Should be reset back to null after
     * the test has finished.
     */
    @VisibleForTesting
    public static void setStartSurfaceEnabledForTesting(@Nullable Boolean isEnabled) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED, isEnabled);
    }

    /**
     * Toggles whether the Duet-TabStrip integration is enabled for testing. Should be reset back to
     * null after the test has finished. Notice that TabGroup should also be turned on in order to
     * really get the feature.
     */
    @VisibleForTesting
    public static void setDuetTabStripIntegrationAndroidEnabledForTesting(
            @Nullable Boolean isEnabled) {
        sFlags.put(ChromePreferenceKeys.FLAGS_CACHED_DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED,
                isEnabled);
    }

    private static boolean isHighEndPhone() {
        return !SysUtils.isLowEndDevice()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    private static boolean isEligibleForTabUiExperiments() {
        return (isTabGroupsAndroidContinuationChromeFlagEnabled() || !SysUtils.isLowEndDevice())
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    /**
     * @return Whether the tab group ui improvement feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidUiImprovementsEnabled() {
        return isTabGroupsAndroidEnabled()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID);
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled() {
        return isTabGroupsAndroidEnabled() && isTabGroupsAndroidContinuationChromeFlagEnabled();
    }

    /**
     * @return Whether the tab group continuation Chrome flag is enabled.
     */
    public static boolean isTabGroupsAndroidContinuationChromeFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
    }

    /**
     * @return Whether the tab strip and duet integration feature is enabled and available for use.
     */
    public static boolean isDuetTabStripIntegrationAndroidEnabled() {
        return isTabGroupsAndroidEnabled()
                && isFlagEnabled(
                        ChromePreferenceKeys.FLAGS_CACHED_DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED,
                        false);
    }

    /**
     * @return Whether this device is running Android Go. This is assumed when we're running Android
     * O or later and we're on a low-end device.
     */
    public static boolean isAndroidGo() {
        return SysUtils.isLowEndDevice()
                && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O;
    }

    /**
     * Cache whether or not bootstrap tasks should be prioritized so on next startup, the value
     * can be made available immediately.
     */
    public static void cachePrioritizeBootstrapTasks() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_PRIORITIZE_BOOTSTRAP_TASKS,
                ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS);
    }

    /**
     * @return Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task
     *         prioritization experiment is enabled).
     */
    public static boolean shouldPrioritizeBootstrapTasks() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_PRIORITIZE_BOOTSTRAP_TASKS, true);
    }

    /**
     * Cache whether warming up network service process is enabled, so that the value
     * can be made available immediately on next start up.
     */
    private static void cacheNetworkServiceWarmUpEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED,
                FeatureUtilitiesJni.get().isNetworkServiceWarmUpEnabled());
    }

    /**
     * @return whether warming up network service is enabled.
     */
    public static boolean isNetworkServiceWarmUpEnabled() {
        return isFlagEnabled(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED, false);
    }

    private static void cacheImmersiveUiModeEnabled() {
        cacheFlag(ChromePreferenceKeys.FLAGS_CACHED_IMMERSIVE_UI_MODE_ENABLED,
                ChromeFeatureList.IMMERSIVE_UI_MODE);
    }

    /**
     * @return Whether immersive ui mode is enabled.
     */
    public static boolean isImmersiveUiModeEnabled() {
        return isFlagEnabled(ChromePreferenceKeys.FLAGS_CACHED_IMMERSIVE_UI_MODE_ENABLED, false);
    }

    /**
     * Returns whether to use {@link Window#setFormat()} to undo opacity change caused by
     * {@link Activity#convertFromTranslucent()}.
     */
    public static boolean isSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                true);
    }

    public static void cacheSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        cacheFlag(
                ChromePreferenceKeys.FLAGS_CACHED_SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT);
    }

    /**
     * Caches the trial group of the reached code profiler feature to be using on next startup.
     */
    private static void cacheReachedCodeProfilerTrialGroup() {
        // Make sure that the existing value is saved in a static variable before overwriting it.
        if (sReachedCodeProfilerTrialGroup == null) {
            getReachedCodeProfilerTrialGroup();
        }

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP,
                FieldTrialList.findFullName(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * @return The trial group of the reached code profiler.
     */
    @CalledByNative
    public static String getReachedCodeProfilerTrialGroup() {
        if (sReachedCodeProfilerTrialGroup == null) {
            sReachedCodeProfilerTrialGroup = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP, "");
        }

        return sReachedCodeProfilerTrialGroup;
    }

    /**
     * @return Whether the thumbnail_aspect_ratio field trail is set.
     */
    public static boolean isTabThumbnailAspectRatioNotOne() {
        if (sEnabledTabThumbnailApsectRatioForTesting != null) {
            return sEnabledTabThumbnailApsectRatioForTesting;
        }

        double expectedAspectRatio = ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, "thumbnail_aspect_ratio", 1.0);
        return Double.compare(1.0, expectedAspectRatio) != 0;
    }

    /**
     * @return Whether to allow to refetch tab thumbnail if the aspect ratio is not matching.
     */
    public static boolean isAllowToRefetchTabThumbnail() {
        return isFlagEnabled(getPrefForFieldTrialParam(
                                     ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, ALLOW_TO_REFETCH),
                false);
    }

    /**
     * Caches the feature flag for whether to allow refetch tab thumbnail if the aspect ratio is not
     * matching.
     */
    private static void cacheAllowToRefetchTabThumbnail() {
        boolean isAllowToRefetchTabThumbnail =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, ALLOW_TO_REFETCH, false);
        SharedPreferencesManager.getInstance().writeBoolean(
                getPrefForFieldTrialParam(
                        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, ALLOW_TO_REFETCH),
                isAllowToRefetchTabThumbnail);
    }

    public static void enableTabThumbnailAspectRatioForTesting(Boolean enabled) {
        sEnabledTabThumbnailApsectRatioForTesting = enabled;
    }

    /**
     * @deprecated Add the feature to {@link #cacheNativeFlags() instead}.
     */
    @Deprecated
    private static void cacheFlag(String preferenceName, String featureName) {
        SharedPreferencesManager.getInstance().writeBoolean(
                preferenceName, ChromeFeatureList.isEnabled(featureName));
    }

    private static void cacheVariation(
            String preferenceName, String featureName, String variationName) {
        SharedPreferencesManager.getInstance().writeString(preferenceName,
                ChromeFeatureList.getFieldTrialParamByFeature(featureName, variationName));
    }

    /**
     * @deprecated Call {@link #isEnabled(String)} instead.
     */
    @Deprecated
    private static boolean isFlagEnabled(String preferenceName, boolean defaultValue) {
        Boolean flag = sFlags.get(preferenceName);
        if (flag == null) {
            flag = SharedPreferencesManager.getInstance().readBoolean(preferenceName, defaultValue);
            sFlags.put(preferenceName, flag);
        }
        return flag;
    }

    private static String getPrefForFeatureFlag(String featureName) {
        return ChromePreferenceKeys.FLAGS_CACHED.createKey(featureName);
    }

    private static String getPrefForFieldTrialParam(String featureName, String paramName) {
        return ChromePreferenceKeys.FLAGS_FIELD_TRIAL_PARAM_CACHED.createKey(
                featureName + ":" + paramName);
    }

    @VisibleForTesting
    public static void resetFlagsForTesting() {
        sFlags.clear();
    }

    @VisibleForTesting
    public static Map<String, Boolean> swapDefaultsForTesting(Map<String, Boolean> testDefaults) {
        Map<String, Boolean> swapped = sDefaults;
        sDefaults = testDefaults;
        return swapped;
    }

    @NativeMethods
    interface Natives {
        boolean isNetworkServiceWarmUpEnabled();
    }
}

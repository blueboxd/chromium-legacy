// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.FieldTrialList;
import org.chromium.base.SysUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarConfiguration;

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
public class CachedFeatureFlags {
    /**
     * Stores the default values for each feature flag queried, used as a fallback in case native
     * isn't loaded, and no value has been previously cached.
     */
    private static Map<String, Boolean> sDefaults = new HashMap<String, Boolean>() {
        {
            put(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY, false);
            put(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD, false);
            put(ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH, false);
            put(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, false);
            put(ChromeFeatureList.CHROME_DUET, false);
            put(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED, false);
            put(ChromeFeatureList.CHROME_DUET_ADAPTIVE, true);
            put(ChromeFeatureList.CHROME_DUET_LABELED, false);
            put(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE, true);
            put(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS, true);
            put(ChromeFeatureList.IMMERSIVE_UI_MODE, false);
            put(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT, true);
            put(ChromeFeatureList.START_SURFACE_ANDROID, false);
            put(ChromeFeatureList.PAINT_PREVIEW_TEST, false);
            put(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, false);
            put(ChromeFeatureList.TAB_GROUPS_ANDROID, false);
            put(ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID, false);
        }
    };

    /**
     * Non-dynamic preference keys used historically for specific features.
     *
     * Do not add new values to this list. To add a new cached feature flag, just follow the
     * instructions in the class javadoc.
     */
    private static final Map<String, String> sNonDynamicPrefKeys = new HashMap<String, String>() {
        {
            put(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD,
                    ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION);
            put(ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH,
                    ChromePreferenceKeys.FLAGS_CACHED_SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH);
            put(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                    ChromePreferenceKeys.FLAGS_CACHED_INTEREST_FEED_CONTENT_SUGGESTIONS);
            put(ChromeFeatureList.CHROME_DUET,
                    ChromePreferenceKeys.FLAGS_CACHED_BOTTOM_TOOLBAR_ENABLED);
            put(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED,
                    ChromePreferenceKeys.FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED);
            put(ChromeFeatureList.CHROME_DUET_ADAPTIVE,
                    ChromePreferenceKeys.FLAGS_CACHED_ADAPTIVE_TOOLBAR_ENABLED);
            put(ChromeFeatureList.CHROME_DUET_LABELED,
                    ChromePreferenceKeys.FLAGS_CACHED_LABELED_BOTTOM_TOOLBAR_ENABLED);
            put(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE,
                    ChromePreferenceKeys.FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE);
            put(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS,
                    ChromePreferenceKeys.FLAGS_CACHED_PRIORITIZE_BOOTSTRAP_TASKS);
            put(ChromeFeatureList.IMMERSIVE_UI_MODE,
                    ChromePreferenceKeys.FLAGS_CACHED_IMMERSIVE_UI_MODE_ENABLED);
            put(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                    ChromePreferenceKeys
                            .FLAGS_CACHED_SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT);
            put(ChromeFeatureList.START_SURFACE_ANDROID,
                    ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED);
            put(ChromeFeatureList.PAINT_PREVIEW_TEST,
                    ChromePreferenceKeys.FLAGS_CACHED_PAINT_PREVIEW_TEST_ENABLED_KEY);
            put(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    ChromePreferenceKeys.FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED);
            put(ChromeFeatureList.TAB_GROUPS_ANDROID,
                    ChromePreferenceKeys.FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED);
            put(ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                    ChromePreferenceKeys.FLAGS_CACHED_DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED);
        }
    };

    private static Map<String, Boolean> sBoolValuesReturned = new HashMap<>();
    private static Map<String, String> sStringValuesReturned = new HashMap<>();
    private static String sReachedCodeProfilerTrialGroup;
    private static Boolean sEnabledTabThumbnailApsectRatioForTesting;

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
    @CalledByNative
    public static boolean isEnabled(String featureName) {
        // All cached feature flags should have a default value.
        if (!sDefaults.containsKey(featureName)) {
            throw new IllegalArgumentException(
                    "Feature " + featureName + " has no default in CachedFeatureFlags.");
        }

        String preferenceName = getPrefForFeatureFlag(featureName);

        Boolean flag = sBoolValuesReturned.get(preferenceName);
        if (flag != null) {
            return flag;
        }

        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        if (prefs.contains(preferenceName)) {
            flag = prefs.readBoolean(preferenceName, false);
        } else {
            flag = sDefaults.get(featureName);
        }
        sBoolValuesReturned.put(preferenceName, flag);
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
        sBoolValuesReturned.put(preferenceName, value);
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags(List<String> featuresToCache) {
        for (String featureName : featuresToCache) {
            if (!sDefaults.containsKey(featureName)) {
                throw new IllegalArgumentException(
                        "Feature " + featureName + " has no default in CachedFeatureFlags.");
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
        cacheNetworkServiceWarmUpEnabled();
        cacheReachedCodeProfilerTrialGroup();
        cacheStartSurfaceVariation();

        // Propagate REACHED_CODE_PROFILER feature value to LibraryLoader. This can't be done in
        // LibraryLoader itself because it lives in //base and can't depend on ChromeFeatureList.
        LibraryLoader.setReachedCodeProfilerEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheFieldTrialParameters(List<CachedFieldTrialParameter> parameters) {
        for (CachedFieldTrialParameter parameter : parameters) {
            parameter.cacheToDisk();
        }
    }

    /**
     * TODO(crbug.com/1012975): Move this to BooleanCachedFieldTrialParameter when
     * CachedFeatureFlags is in chrome/browser/flags.
     *
     * @return the value of the field trial parameter that should be used in this run.
     */
    public static boolean getValue(BooleanCachedFieldTrialParameter parameter) {
        return getConsistentBooleanValue(
                parameter.getSharedPreferenceKey(), parameter.getDefaultValue());
    }

    /**
     * TODO(crbug.com/1012975): Move this to StringCachedFieldTrialParameter when
     * CachedFeatureFlags is in chrome/browser/flags.
     *
     * @return the value of the field trial parameter that should be used in this run.
     */
    public static String getValue(StringCachedFieldTrialParameter parameter) {
        return getConsistentStringValue(
                parameter.getSharedPreferenceKey(), parameter.getDefaultValue());
    }

    public static boolean isCommandLineOnNonRootedEnabled() {
        return isEnabled(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
    }

    private static void cacheStartSurfaceVariation() {
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
        return isEnabled(ChromeFeatureList.START_SURFACE_ANDROID) && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Paint Preview capture test is enabled.
     */
    public static boolean isPaintPreviewTestEnabled() {
        return isEnabled(ChromeFeatureList.PAINT_PREVIEW_TEST);
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        return isStartSurfaceEnabled()
                && getConsistentBooleanValue(
                        ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY, false);
    }

    /**
     * Toggles whether the StartSurface is enabled for testing. Should be reset back to null after
     * the test has finished.
     */
    @VisibleForTesting
    public static void setStartSurfaceEnabledForTesting(@Nullable Boolean isEnabled) {
        setForTesting(ChromeFeatureList.START_SURFACE_ANDROID, isEnabled);
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
     * @return Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task
     *         prioritization experiment is enabled).
     */
    public static boolean shouldPrioritizeBootstrapTasks() {
        return isEnabled(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS);
    }

    /**
     * Cache whether warming up network service process is enabled, so that the value
     * can be made available immediately on next start up.
     */
    private static void cacheNetworkServiceWarmUpEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED,
                CachedFeatureFlagsJni.get().isNetworkServiceWarmUpEnabled());
    }

    /**
     * @return whether warming up network service is enabled.
     */
    public static boolean isNetworkServiceWarmUpEnabled() {
        return getConsistentBooleanValue(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED, false);
    }

    /**
     * @return Whether immersive ui mode is enabled.
     */
    public static boolean isImmersiveUiModeEnabled() {
        return isEnabled(ChromeFeatureList.IMMERSIVE_UI_MODE);
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

    public static void enableTabThumbnailAspectRatioForTesting(Boolean enabled) {
        sEnabledTabThumbnailApsectRatioForTesting = enabled;
    }

    static boolean getConsistentBooleanValue(String preferenceName, boolean defaultValue) {
        Boolean flag = sBoolValuesReturned.get(preferenceName);
        if (flag == null) {
            flag = SharedPreferencesManager.getInstance().readBoolean(preferenceName, defaultValue);
            sBoolValuesReturned.put(preferenceName, flag);
        }
        return flag;
    }

    static String getConsistentStringValue(String preferenceName, String defaultValue) {
        String value = sStringValuesReturned.get(preferenceName);
        if (value == null) {
            value = SharedPreferencesManager.getInstance().readString(preferenceName, defaultValue);
            sStringValuesReturned.put(preferenceName, value);
        }
        return value;
    }

    private static String getPrefForFeatureFlag(String featureName) {
        String grandfatheredPrefKey = sNonDynamicPrefKeys.get(featureName);
        if (grandfatheredPrefKey == null) {
            return ChromePreferenceKeys.FLAGS_CACHED.createKey(featureName);
        } else {
            return grandfatheredPrefKey;
        }
    }

    @VisibleForTesting
    public static void resetFlagsForTesting() {
        sBoolValuesReturned.clear();
        sStringValuesReturned.clear();
    }

    @VisibleForTesting
    public static Map<String, Boolean> swapDefaultsForTesting(Map<String, Boolean> testDefaults) {
        Map<String, Boolean> swapped = sDefaults;
        sDefaults = testDefaults;
        return swapped;
    }

    @Deprecated
    public static boolean isBottomToolbarEnabled() {
        // TODO(crbug.com/1012975): Remove this when downstream calls BottomToolbarConfiguration.
        return BottomToolbarConfiguration.isBottomToolbarEnabled();
    }

    @NativeMethods
    interface Natives {
        boolean isNetworkServiceWarmUpEnabled();
    }
}

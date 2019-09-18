// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;
import android.speech.RecognizerIntent;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.touchless.TouchlessDelegate;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * A utility {@code class} meant to help determine whether or not certain features are supported by
 * this device.
 *
 * This utility class also contains support for cached feature flags that must take effect on
 * startup before native is initialized but are set via native code. The caching is done in
 * {@link android.content.SharedPreferences}, which is available in Java immediately.
 *
 * When adding a new cached flag, it is common practice to use a static Boolean in this file to
 * track whether the feature is enabled. A static method that returns the static Boolean can
 * then be added to this file allowing client code to query whether the feature is enabled. The
 * first time the method is called, the static Boolean should be set to the corresponding shared
 * preference. After native is initialized, the shared preference will be updated to reflect the
 * native flag value (e.g. the actual experimental feature flag value).
 *
 * When using a cached flag, the static Boolean should be the source of truth for whether the
 * feature is turned on for the current session. As such, always rely on the static Boolean
 * when determining whether the corresponding experimental behavior should be enabled. When
 * querying whether a cached feature is enabled from native, an @CalledByNative method can be
 * exposed in this file to allow feature_utilities.cc to retrieve the cached value.
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class FeatureUtilities {
    private static final String TAG = "FeatureUtilities";

    private static Boolean sHasGoogleAccountAuthenticator;
    private static Boolean sHasRecognitionIntentHandler;

    private static Boolean sIsBottomToolbarEnabled;
    private static Boolean sIsAdaptiveToolbarEnabled;
    private static Boolean sIsLabeledBottomToolbarEnabled;
    private static Boolean sIsNightModeAvailable;
    private static Boolean sIsNightModeForCustomTabsAvailable;
    private static Boolean sShouldPrioritizeBootstrapTasks;
    private static Boolean sIsGridTabSwitcherEnabled;
    private static Boolean sIsStartSurfaceEnabled;
    private static Boolean sIsTabGroupsAndroidEnabled;
    private static Boolean sIsTabToGtsAnimationEnabled;
    private static Boolean sFeedEnabled;
    private static Boolean sServiceManagerForBackgroundPrefetch;
    private static Boolean sIsNetworkServiceWarmUpEnabled;
    private static Boolean sIsImmersiveUiModeEnabled;
    private static Boolean sServiceManagerForDownloadResumption;
    private static Boolean sIsClickToCallOpenDialerDirectlyEnabled;

    private static Boolean sDownloadAutoResumptionEnabledInNative;

    private static String sReachedCodeProfilerTrialGroup;

    /**
     * Determines whether or not the {@link RecognizerIntent#ACTION_WEB_SEARCH} {@link Intent}
     * is handled by any {@link android.app.Activity}s in the system.  The result will be cached for
     * future calls.  Passing {@code false} to {@code useCachedValue} will force it to re-query any
     * {@link android.app.Activity}s that can process the {@link Intent}.
     * @param context        The {@link Context} to use to check to see if the {@link Intent} will
     *                       be handled.
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    public static boolean isRecognitionIntentPresent(Context context, boolean useCachedValue) {
        ThreadUtils.assertOnUiThread();
        if (sHasRecognitionIntentHandler == null || !useCachedValue) {
            PackageManager pm = context.getPackageManager();
            List<ResolveInfo> activities = pm.queryIntentActivities(
                    new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH), 0);
            sHasRecognitionIntentHandler = activities.size() > 0;
        }

        return sHasRecognitionIntentHandler;
    }

    /**
     * Determines whether or not the user has a Google account (so we can sync) or can add one.
     * @return Whether or not sync is allowed on this device.
     */
    public static boolean canAllowSync() {
        return (hasGoogleAccountAuthenticator() && hasSyncPermissions()) || hasGoogleAccounts();
    }

    @VisibleForTesting
    static boolean hasGoogleAccountAuthenticator() {
        if (sHasGoogleAccountAuthenticator == null) {
            AccountManagerFacade accountHelper = AccountManagerFacade.get();
            sHasGoogleAccountAuthenticator = accountHelper.hasGoogleAccountAuthenticator();
        }
        return sHasGoogleAccountAuthenticator;
    }

    @VisibleForTesting
    static boolean hasGoogleAccounts() {
        return AccountManagerFacade.get().hasGoogleAccounts();
    }

    @SuppressLint("InlinedApi")
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static boolean hasSyncPermissions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return true;

        UserManager manager = (UserManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USER_SERVICE);
        Bundle userRestrictions = manager.getUserRestrictions();
        return !userRestrictions.getBoolean(UserManager.DISALLOW_MODIFY_ACCOUNTS, false);
    }

    /**
     * Records the current custom tab visibility state with native-side feature utilities.
     * @param visible Whether a custom tab is visible.
     */
    public static void setCustomTabVisible(boolean visible) {
        FeatureUtilitiesJni.get().setCustomTabVisible(visible);
    }

    /**
     * Records whether the activity is in multi-window mode with native-side feature utilities.
     * @param isInMultiWindowMode Whether the activity is in Android N multi-window mode.
     */
    public static void setIsInMultiWindowMode(boolean isInMultiWindowMode) {
        FeatureUtilitiesJni.get().setIsInMultiWindowMode(isInMultiWindowMode);
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags() {
        cacheCommandLineOnNonRootedEnabled();
        FirstRunUtils.cacheFirstRunPrefs();
        cacheBottomToolbarEnabled();
        cacheAdaptiveToolbarEnabled();
        cacheLabeledBottomToolbarEnabled();
        cacheNightModeAvailable();
        cacheNightModeForCustomTabsAvailable();
        cacheDownloadAutoResumptionEnabledInNative();
        cachePrioritizeBootstrapTasks();
        cacheFeedEnabled();
        cacheNetworkServiceWarmUpEnabled();
        cacheImmersiveUiModeEnabled();
        cacheSwapPixelFormatToFixConvertFromTranslucentEnabled();
        cacheReachedCodeProfilerTrialGroup();
        cacheStartSurfaceEnabled();
        cacheClickToCallOpenDialerDirectlyEnabled();

        if (isHighEndPhone()) cacheGridTabSwitcherEnabled();
        if (isHighEndPhone()) cacheTabGroupsAndroidEnabled();

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
        boolean resumptionDownloadInReducedMode =
                ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD);

        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY,
                resumptionDownloadInReducedMode);
    }

    /**
     * @return if DownloadResumptionBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForDownloadResumptionEnabled() {
        if (sServiceManagerForDownloadResumption == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sServiceManagerForDownloadResumption = prefManager.readBoolean(
                    ChromePreferenceManager.SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY, false);
        }
        return sServiceManagerForDownloadResumption;
    }

    public static void cacheServiceManagerForBackgroundPrefetch() {
        boolean backgroundPrefetchInReducedMode = ChromeFeatureList.isEnabled(
                ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH);

        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY,
                backgroundPrefetchInReducedMode);
    }

    /**
     * @return if PrefetchBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForBackgroundPrefetchEnabled() {
        if (sServiceManagerForBackgroundPrefetch == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sServiceManagerForBackgroundPrefetch = prefManager.readBoolean(
                    ChromePreferenceManager.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY, false);
        }
        return sServiceManagerForBackgroundPrefetch && isFeedEnabled();
    }

    /**
     * Cache the value of the flag whether or not to use Feed so it can be checked in Java before
     * native is loaded.
     */
    public static void cacheFeedEnabled() {
        boolean feedEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS);

        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.INTEREST_FEED_CONTENT_SUGGESTIONS_KEY, feedEnabled);
        sFeedEnabled = feedEnabled;
    }

    /**
     * @return Whether or not the Feed is enabled (based on the cached value in SharedPrefs).
     */
    public static boolean isFeedEnabled() {
        if (sFeedEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sFeedEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.INTEREST_FEED_CONTENT_SUGGESTIONS_KEY, false);
        }
        return sFeedEnabled;
    }

    /**
     * @return Whether or not the download auto-resumptions should be enabled in native.
     */
    @CalledByNative
    public static boolean isDownloadAutoResumptionEnabledInNative() {
        if (sDownloadAutoResumptionEnabledInNative == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sDownloadAutoResumptionEnabledInNative = prefManager.readBoolean(
                    ChromePreferenceManager.DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY, true);
        }
        return sDownloadAutoResumptionEnabledInNative;
    }

    /**
     * Cache whether or not the bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheBottomToolbarEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.BOTTOM_TOOLBAR_ENABLED_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_DUET));
    }

    /**
     * Cache whether or not the adaptive toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheAdaptiveToolbarEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.ADAPTIVE_TOOLBAR_ENABLED_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_DUET_ADAPTIVE));
    }

    /**
     * Cache whether or not the labeled bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheLabeledBottomToolbarEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.LABELED_BOTTOM_TOOLBAR_ENABLED_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_DUET_LABELED));
    }

    /**
     * Cache whether or not download auto-resumptions are enabled in native so on next startup, the
     * value can be made available immediately.
     */
    private static void cacheDownloadAutoResumptionEnabledInNative() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE));
    }

    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        if (sIsBottomToolbarEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsBottomToolbarEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.BOTTOM_TOOLBAR_ENABLED_KEY, false);
        }
        // TODO(crbug.com/944228): TabGroupsAndroid and ChromeDuet are incompatible for now.
        return sIsBottomToolbarEnabled
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext())
                && !isTabGroupsAndroidEnabled();
    }

    /**
     * @return Whether or not the adaptive toolbar is enabled.
     */
    public static boolean isAdaptiveToolbarEnabled() {
        if (sIsAdaptiveToolbarEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsAdaptiveToolbarEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.ADAPTIVE_TOOLBAR_ENABLED_KEY, true);
        }
        return sIsAdaptiveToolbarEnabled && isBottomToolbarEnabled() && !isGridTabSwitcherEnabled();
    }

    /**
     * @return Whether or not the labeled bottom toolbar is enabled.
     */
    public static boolean isLabeledBottomToolbarEnabled() {
        if (sIsLabeledBottomToolbarEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsLabeledBottomToolbarEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.LABELED_BOTTOM_TOOLBAR_ENABLED_KEY, false);
        }
        return sIsLabeledBottomToolbarEnabled && isBottomToolbarEnabled();
    }

    /**
     * Cache whether or not night mode is available (i.e. night mode experiment is enabled) so on
     * next startup, the value can be made available immediately.
     */
    public static void cacheNightModeAvailable() {
        boolean available = ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE)
                || (BuildInfo.isAtLeastQ()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE_FOR_Q));
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NIGHT_MODE_AVAILABLE_KEY, available);
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled).
     */
    public static boolean isNightModeAvailable() {
        if (sIsNightModeAvailable == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsNightModeAvailable = prefManager.readBoolean(
                    ChromePreferenceManager.NIGHT_MODE_AVAILABLE_KEY, false);
        }
        return sIsNightModeAvailable;
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeAvailableForTesting(@Nullable Boolean available) {
        sIsNightModeAvailable = available;
    }

    /**
     * Cache whether or not night mode is available for custom tabs (i.e. night mode experiment is
     * enabled), so the value is immediately available on next start-up.
     */
    public static void cacheNightModeForCustomTabsAvailable() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NIGHT_MODE_CCT_AVAILABLE_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE_CCT));
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled) for custom tabs.
     */
    public static boolean isNightModeForCustomTabsAvailable() {
        if (sIsNightModeForCustomTabsAvailable == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsNightModeForCustomTabsAvailable = prefManager.readBoolean(
                    ChromePreferenceManager.NIGHT_MODE_CCT_AVAILABLE_KEY, true);
        }
        return sIsNightModeForCustomTabsAvailable;
    }

    /**
     * Toggles whether the night mode for custom tabs experiment is enabled. Must only be used for
     * testing. Should be reset back to NULL after the test has finished.
     */
    public static void setNightModeForCustomTabsAvailableForTesting(Boolean available) {
        sIsNightModeForCustomTabsAvailable = available;
    }

    /**
     * Cache whether or not command line is enabled on non-rooted devices.
     */
    private static void cacheCommandLineOnNonRootedEnabled() {
        boolean isCommandLineOnNonRootedEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
        ChromePreferenceManager manager = ChromePreferenceManager.getInstance();
        manager.writeBoolean(ChromePreferenceManager.COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY,
                isCommandLineOnNonRootedEnabled);
    }

    /**
     * @return Whether or not the download progress infobar is enabled.
     */
    public static boolean isDownloadProgressInfoBarEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR);
    }

    private static void cacheStartSurfaceEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.START_SURFACE_ENABLED_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.START_SURFACE_ANDROID));
    }

    /**
     * @return Whether the Start Surface is enabled.
     */
    public static boolean isStartSurfaceEnabled() {
        if (sIsStartSurfaceEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsStartSurfaceEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.START_SURFACE_ENABLED_KEY, false);
        }
        return sIsStartSurfaceEnabled;
    }

    private static void cacheGridTabSwitcherEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.GRID_TAB_SWITCHER_ENABLED_KEY,
                !DeviceClassManager.enableAccessibilityLayout()
                        && (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.DOWNLOAD_TAB_MANAGEMENT_MODULE)
                                || ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID))
                        && TabManagementModuleProvider.getDelegate() != null
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID));
    }

    /**
     * @return Whether the Grid Tab Switcher UI is enabled and available for use.
     */
    public static boolean isGridTabSwitcherEnabled() {
        if (sIsGridTabSwitcherEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sIsGridTabSwitcherEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.GRID_TAB_SWITCHER_ENABLED_KEY, false);
        }
        // TODO(yusufo): AccessibilityLayout check should not be here and the flow should support
        // changing that setting while Chrome is alive.
        // Having Tab Groups implies Grid Tab Switcher.
        return sIsGridTabSwitcherEnabled || isTabGroupsAndroidEnabled();
    }

    /**
     * Toggles whether the Grid Tab Switcher is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setGridTabSwitcherEnabledForTesting(@Nullable Boolean enabled) {
        sIsGridTabSwitcherEnabled = enabled;
    }

    private static void cacheTabGroupsAndroidEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.TAB_GROUPS_ANDROID_ENABLED_KEY,
                !DeviceClassManager.enableAccessibilityLayout()
                        && (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.DOWNLOAD_TAB_MANAGEMENT_MODULE)
                                || ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.TAB_GROUPS_ANDROID))
                        && TabManagementModuleProvider.getDelegate() != null
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID));
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
        if (sIsTabGroupsAndroidEnabled == null) {
            ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance();

            sIsTabGroupsAndroidEnabled = preferenceManager.readBoolean(
                    ChromePreferenceManager.TAB_GROUPS_ANDROID_ENABLED_KEY, false);
            sIsTabGroupsAndroidEnabled &= isHighEndPhone();
        }

        return sIsTabGroupsAndroidEnabled;
    }

    /**
     * Toggles whether the Tab Group is enabled for testing. Should be reset back to null after the
     * test has finished.
     */
    @VisibleForTesting
    public static void setTabGroupsAndroidEnabledForTesting(@Nullable Boolean available) {
        sIsTabGroupsAndroidEnabled = available;
    }

    /**
     * Toggles whether the Tab-to-GTS animation is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setIsTabToGtsAnimationEnabledForTesting(@Nullable Boolean enabled) {
        sIsTabToGtsAnimationEnabled = enabled;
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled() {
        if (sIsTabToGtsAnimationEnabled != null) {
            Log.d(TAG, "IsTabToGtsAnimationEnabled forced to " + sIsTabToGtsAnimationEnabled);
            return sIsTabToGtsAnimationEnabled;
        }
        Log.d(TAG, "GTS.MinSdkVersion = " + GridTabSwitcherUtil.getMinSdkVersion());
        Log.d(TAG, "GTS.MinMemoryMB = " + GridTabSwitcherUtil.getMinMemoryMB());
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                && Build.VERSION.SDK_INT >= GridTabSwitcherUtil.getMinSdkVersion()
                && SysUtils.amountOfPhysicalMemoryKB() / 1024
                >= GridTabSwitcherUtil.getMinMemoryMB();
    }

    private static boolean isHighEndPhone() {
        return !SysUtils.isLowEndDevice()
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
     * @return Whether this device is running Android Go. This is assumed when we're running Android
     * O or later and we're on a low-end device.
     */
    public static boolean isAndroidGo() {
        return SysUtils.isLowEndDevice()
                && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O;
    }

    /**
     * @return Whether no-touch-mode is enabled.
     */
    @CalledByNative
    public static boolean isNoTouchModeEnabled() {
        return TouchlessDelegate.TOUCHLESS_MODE_ENABLED;
    }

    /**
     * Cache whether or not bootstrap tasks should be prioritized so on next startup, the value
     * can be made available immediately.
     */
    public static void cachePrioritizeBootstrapTasks() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.PRIORITIZE_BOOTSTRAP_TASKS_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS));
    }

    /**
     * @return Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task
     *         prioritization experiment is enabled).
     */
    public static boolean shouldPrioritizeBootstrapTasks() {
        if (sShouldPrioritizeBootstrapTasks == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();

            sShouldPrioritizeBootstrapTasks = prefManager.readBoolean(
                    ChromePreferenceManager.PRIORITIZE_BOOTSTRAP_TASKS_KEY, true);
        }
        return sShouldPrioritizeBootstrapTasks;
    }

    /**
     * Cache whether warming up network service process is enabled, so that the value
     * can be made available immediately on next start up.
     */
    private static void cacheNetworkServiceWarmUpEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NETWORK_SERVICE_WARM_UP_ENABLED_KEY,
                FeatureUtilitiesJni.get().isNetworkServiceWarmUpEnabled());
    }

    /**
     * @return whether warming up network service is enabled.
     */
    public static boolean isNetworkServiceWarmUpEnabled() {
        if (sIsNetworkServiceWarmUpEnabled == null) {
            ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();
            sIsNetworkServiceWarmUpEnabled = prefManager.readBoolean(
                    ChromePreferenceManager.NETWORK_SERVICE_WARM_UP_ENABLED_KEY, false);
        }
        return sIsNetworkServiceWarmUpEnabled;
    }

    private static void cacheImmersiveUiModeEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.IMMERSIVE_UI_MODE_ENABLED,
                ChromeFeatureList.isEnabled(ChromeFeatureList.IMMERSIVE_UI_MODE));
    }

    /**
     * @return Whether immersive ui mode is enabled.
     */
    public static boolean isImmersiveUiModeEnabled() {
        if (sIsImmersiveUiModeEnabled == null) {
            sIsImmersiveUiModeEnabled = ChromePreferenceManager.getInstance().readBoolean(
                    ChromePreferenceManager.IMMERSIVE_UI_MODE_ENABLED, false);
        }

        return sIsImmersiveUiModeEnabled;
    }

    /**
     * Returns whether to use {@link Window#setFormat()} to undo opacity change caused by
     * {@link Activity#convertFromTranslucent()}.
     */
    public static boolean isSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        return ChromePreferenceManager.getInstance().readBoolean(
                ChromePreferenceManager.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT, true);
    }

    public static void cacheSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT));
    }

    /**
     * Cache the value of the flag whether or not to directly open the dialer for click to call.
     */
    public static void cacheClickToCallOpenDialerDirectlyEnabled() {
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.CLICK_TO_CALL_OPEN_DIALER_DIRECTLY));
    }

    /**
     * @return Whether or not we should directly open dialer for click to call (based on the cached
     *         value in SharedPrefs).
     */
    public static boolean isClickToCallOpenDialerDirectlyEnabled() {
        if (sIsClickToCallOpenDialerDirectlyEnabled == null) {
            sIsClickToCallOpenDialerDirectlyEnabled =
                    ChromePreferenceManager.getInstance().readBoolean(
                            ChromePreferenceManager.CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY, false);
        }
        return sIsClickToCallOpenDialerDirectlyEnabled;
    }

    /**
     * Toggles whether experiment for opening dialer directly in click to call is enabled for
     * testing. Should be reset back to null after the test has finished.
     */
    @VisibleForTesting
    public static void setIsClickToCallOpenDialerDirectlyEnabledForTesting(
            @Nullable Boolean isEnabled) {
        sIsClickToCallOpenDialerDirectlyEnabled = isEnabled;
    }

    /**
     * Caches the trial group of the reached code profiler feature to be using on next startup.
     */
    private static void cacheReachedCodeProfilerTrialGroup() {
        // Make sure that the existing value is saved in a static variable before overwriting it.
        if (sReachedCodeProfilerTrialGroup == null) {
            getReachedCodeProfilerTrialGroup();
        }

        ChromePreferenceManager.getInstance().writeString(
                ChromePreferenceManager.REACHED_CODE_PROFILER_GROUP_KEY,
                FieldTrialList.findFullName(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * @return The trial group of the reached code profiler.
     */
    @CalledByNative
    public static String getReachedCodeProfilerTrialGroup() {
        if (sReachedCodeProfilerTrialGroup == null) {
            sReachedCodeProfilerTrialGroup = ChromePreferenceManager.getInstance().readString(
                    ChromePreferenceManager.REACHED_CODE_PROFILER_GROUP_KEY, "");
        }

        return sReachedCodeProfilerTrialGroup;
    }

    private static class GridTabSwitcherUtil {
        // Field trial parameter for the minimum Android SDK version to enable zooming animation.
        private static final String MIN_SDK_PARAM = "zooming-min-sdk-version";
        private static final int DEFAULT_MIN_SDK = Build.VERSION_CODES.O;

        // Field trial parameter for the minimum physical memory size to enable zooming animation.
        private static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
        private static final int DEFAULT_MIN_MEMORY_MB = 2048;

        private static int getMinSdkVersion() {
            String sdkVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_SDK_PARAM);
            try {
                return Integer.valueOf(sdkVersion);
            } catch (NumberFormatException e) {
                return DEFAULT_MIN_SDK;
            }
        }

        private static int getMinMemoryMB() {
            String sdkVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_MEMORY_MB_PARAM);
            try {
                return Integer.valueOf(sdkVersion);
            } catch (NumberFormatException e) {
                return DEFAULT_MIN_MEMORY_MB;
            }
        }
    }

    @NativeMethods
    interface Natives {
        void setCustomTabVisible(boolean visible);
        void setIsInMultiWindowMode(boolean isInMultiWindowMode);
        boolean isNetworkServiceWarmUpEnabled();
    }
}

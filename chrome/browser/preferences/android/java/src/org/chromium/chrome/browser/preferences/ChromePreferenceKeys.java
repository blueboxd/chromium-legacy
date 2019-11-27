// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.List;

/**
 * Contains String and {@link KeyPrefix} constants with the SharedPreferences keys used by Chrome.
 *
 * All Chrome layer SharedPreferences keys should be declared in this class.
 *
 * To add a new key:
 * 1. Declare it as a String constant in this class. Its value should follow the format
 *    "Chrome.[Feature].[Key]"
 * 2. Add it to createKeysInUse().
 *
 * To deprecate a key that is not used anymore:
 * 1. Add its constant value to createDeprecatedKeysForTesting().
 * 2. Remove the key from createKeysInUse().
 * 3. If the key is in createGrandfatheredFormatKeysForTesting(), remove it from there.
 * 4. Delete the constant.
 *
 * To add a new KeyPrefix:
 * 1. Declare it as a KeyPrefix constant in this class. Its value should follow the format
 *    "Chrome.[Feature].[KeyPrefix].*"
 * 2. Add PREFIX_CONSTANT.pattern() to the list of used keys in
 *    {@link ChromePreferenceKeys#createKeysInUse()} ()}.
 *
 * To deprecate a KeyPrefix that is not used anymore:
 * 1. Add its String value to createDeprecatedKeysForTesting(), including the ".*"
 * 2. Remove it from createKeysInUse().
 * 3. Delete the KeyPrefix constant.
 *
 * Tests in ChromePreferenceKeysTest ensure the sanity of this file.
 */
public final class ChromePreferenceKeys {
    /** An all-time counter of taps that triggered the Contextual Search peeking panel. */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT =
            "contextual_search_all_time_tap_count";
    /** An all-time counter of Contextual Search panel opens triggered by any gesture.*/
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT =
            "contextual_search_all_time_open_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT =
            "contextual_search_all_time_tap_quick_answer_count";
    /**
     * The number of times that a tap triggered the Contextual Search panel to peek since the last
     * time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT =
            "contextual_search_tap_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown since
     * the last time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT =
            "contextual_search_tap_quick_answer_count";
    /**
     * The number of times the Contextual Search panel was opened with the opt-in promo visible.
     */
    public static final String CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT =
            "contextual_search_promo_open_count";
    /**
     * The entity-data impressions count for Contextual Search, i.e. thumbnails shown in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT =
            "contextual_search_entity_impressions_count";
    /**
     * The entity-data opens count for Contextual Search, e.g. Panel opens following thumbnails
     * shown in the Bar. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT =
            "contextual_search_entity_opens_count";
    /**
     * The Quick Action impressions count for Contextual Search, i.e. actions presented in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT =
            "contextual_search_quick_action_impressions_count";
    /**
     * The Quick Actions taken count for Contextual Search, i.e. phone numbers dialed and similar
     * actions. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT =
            "contextual_search_quick_actions_taken_count";
    /**
     * The Quick Actions ignored count, i.e. phone numbers available but not dialed.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT =
            "contextual_search_quick_actions_ignored_count";
    /**
     * A user interaction event ID for interaction with Contextual Search, stored as a long.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID =
            "contextual_search_previous_interaction_event_id";
    /**
     * An encoded set of outcomes of user interaction with Contextual Search, stored as an int.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES =
            "contextual_search_previous_interaction_encoded_outcomes";
    /**
     * A timestamp indicating when we updated the user interaction with Contextual Search, stored
     * as a long, with resolution in days.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP =
            "contextual_search_previous_interaction_timestamp";
    public static final String CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT =
            "contextual_search_tap_triggered_promo_count";
    public static final String CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME =
            "contextual_search_last_animation_time";
    public static final String CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER =
            "contextual_search_current_week_number";

    public static final String CONTEXTUAL_SEARCH_OLDEST_WEEK = "contextual_search_oldest_week";
    public static final String CONTEXTUAL_SEARCH_NEWEST_WEEK = "contextual_search_newest_week";
    public static final String CONTEXTUAL_SEARCH_CLICKS_WEEK_0 = "contextual_search_clicks_week_0";
    public static final String CONTEXTUAL_SEARCH_CLICKS_WEEK_1 = "contextual_search_clicks_week_1";
    public static final String CONTEXTUAL_SEARCH_CLICKS_WEEK_2 = "contextual_search_clicks_week_2";
    public static final String CONTEXTUAL_SEARCH_CLICKS_WEEK_3 = "contextual_search_clicks_week_3";
    public static final String CONTEXTUAL_SEARCH_CLICKS_WEEK_4 = "contextual_search_clicks_week_4";
    public static final String CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_0 =
            "contextual_search_impressions_week_0";
    public static final String CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_1 =
            "contextual_search_impressions_week_1";
    public static final String CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_2 =
            "contextual_search_impressions_week_2";
    public static final String CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_3 =
            "contextual_search_impressions_week_3";
    public static final String CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_4 =
            "contextual_search_impressions_week_4";

    /**
     * Whether the promotion for data reduction has been skipped on first invocation.
     * Default value is false.
     */
    public static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";
    public static final String SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION =
            "signin_promo_last_shown_chrome_version";
    public static final String SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES =
            "signin_promo_last_shown_account_names";
    public static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS =
            "signin_promo_impressions_count_bookmarks";
    public static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS =
            "signin_promo_impressions_count_settings";

    /**
     * Whether Chrome is set as the default browser.
     * Default value is false.
     */
    public static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    /**
     * The current theme setting in the user settings.
     * Default value is -1. Use NightModeUtils#getThemeSetting() to retrieve current setting or
     * default theme.
     */
    public static final String UI_THEME_SETTING_KEY = "ui_theme_setting";

    /**
     * Whether or not darken websites is enabled.
     * Default value is false.
     */
    public static final String DARKEN_WEBSITES_ENABLED_KEY = "darken_websites_enabled";

    /**
     * Marks that the content suggestions surface has been shown.
     * Default value is false.
     */
    public static final String CONTENT_SUGGESTIONS_SHOWN_KEY = "content_suggestions_shown";

    /**
     * Whether the user dismissed the personalized sign in promo from the Settings.
     * Default value is false.
     */
    public static final String SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED =
            "settings_personalized_signin_promo_dismissed";
    /**
     * Whether the user dismissed the personalized sign in promo from the new tab page.
     * Default value is false.
     */
    public static final String NTP_SIGNIN_PROMO_DISMISSED =
            "ntp.personalized_signin_promo_dismissed";

    public static final String NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START =
            "ntp.signin_promo_suppression_period_start";

    public static final String CRASH_UPLOAD_SUCCESS_BROWSER = "browser_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_RENDERER = "renderer_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_GPU = "gpu_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_OTHER = "other_crash_success_upload";
    public static final String CRASH_UPLOAD_FAILURE_BROWSER = "browser_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_RENDERER = "renderer_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_GPU = "gpu_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_OTHER = "other_crash_failure_upload";

    public static final String VERIFIED_DIGITAL_ASSET_LINKS = "verified_digital_asset_links";
    public static final String TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES =
            "trusted_web_activity_disclosure_accepted_packages";

    /**
     * Whether VR assets component should be registered on startup.
     * Default value is false.
     */
    public static final String SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP =
            "should_register_vr_assets_component_on_startup";

    /*
     * Whether the simplified tab switcher is enabled when accessibility mode is enabled. Keep in
     * sync with accessibility_preferences.xml.
     * Default value is true.
     */
    public static final String ACCESSIBILITY_TAB_SWITCHER = "accessibility_tab_switcher";

    /**
     * When the user is shown a badge that the current Android OS version is unsupported, and they
     * tap it to display the menu (which has additional information), we store the current version
     * of Chrome to this preference to ensure we only show the badge once. The value is cleared
     * if the Chrome version later changes.
     */
    public static final String LATEST_UNSUPPORTED_VERSION = "android_os_unsupported_chrome_version";

    /**
     * Keys for deferred recording of the outcomes of showing the clear data dialog after
     * Trusted Web Activity client apps are uninstalled or have their data cleared.
     */
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL =
            "twa_dialog_number_of_dismissals_on_uninstall";
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA =
            "twa_dialog_number_of_dismissals_on_clear_data";

    /** Key for deferred recording of list of uninstalled WebAPK packages. */
    public static final String WEBAPK_UNINSTALLED_PACKAGES = "webapk_uninstalled_packages";

    /**
     * Contains a trial group that was used to determine whether the reached code profiler should be
     * enabled.
     */
    public static final String REACHED_CODE_PROFILER_GROUP_KEY = "reached_code_profiler_group";

    /**
     * Key to cache whether offline indicator v2 (persistent offline indicator) is enabled.
     */
    public static final String OFFLINE_INDICATOR_V2_ENABLED_KEY = "offline_indicator_v2_enabled";

    /**
     * Personalized signin promo preference.
     */
    public static final String PREF_PERSONALIZED_SIGNIN_PROMO_DECLINED =
            "signin_promo_bookmarks_declined";
    /**
     * Generic signin and sync promo preferences.
     */
    public static final String PREF_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT =
            "enhanced_bookmark_signin_promo_show_count";

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Keys representing cached feature flags                                                     //
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Key for whether DownloadResumptionBackgroundTask should load native in service manager only
     * mode.
     * Default value is false.
     */
    public static final String SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY =
            "service_manager_for_download_resumption";

    /**
     * Key for whether PrefetchBackgroundTask should load native in service manager only mode.
     * Default value is false.
     */
    public static final String SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY =
            "service_manager_for_background_prefetch";

    public static final String INTEREST_FEED_CONTENT_SUGGESTIONS_KEY =
            "interest_feed_content_suggestions";

    /**
     * Whether or not the download auto-resumption is enabled in native.
     * Default value is true.
     */
    public static final String DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY =
            "download_auto_resumption_in_native";

    /**
     * Whether or not the bottom toolbar is enabled.
     * Default value is false.
     */
    public static final String BOTTOM_TOOLBAR_ENABLED_KEY = "bottom_toolbar_enabled";

    /**
     * Whether or not the adaptive toolbar is enabled.
     * Default value is true.
     */
    public static final String ADAPTIVE_TOOLBAR_ENABLED_KEY = "adaptive_toolbar_enabled";

    /**
     * Whether or not the labeled bottom toolbar is enabled.
     * Default value is false.
     */
    public static final String LABELED_BOTTOM_TOOLBAR_ENABLED_KEY =
            "labeled_bottom_toolbar_enabled";

    /**
     * Whether or not night mode is available.
     * Default value is false.
     */
    public static final String NIGHT_MODE_AVAILABLE_KEY = "night_mode_available";

    /**
     * Whether or not night mode should set "light" as the default option.
     * Default value is false.
     */
    public static final String NIGHT_MODE_DEFAULT_TO_LIGHT = "night_mode_default_to_light";

    /**
     * Whether or not night mode is available for custom tabs.
     * Default value is false.
     */
    public static final String NIGHT_MODE_CCT_AVAILABLE_KEY = "night_mode_cct_available";

    /**
     * Whether or not command line on non-rooted devices is enabled.
     * Default value is false.
     */
    public static final String COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY =
            "command_line_on_non_rooted_enabled";

    /**
     * Whether or not the start surface is enabled.
     * Default value is false.
     */
    public static final String START_SURFACE_ENABLED_KEY = "start_surface_enabled";

    /**
     * Whether or not the grid tab switcher is enabled.
     * Default value is false.
     */
    public static final String GRID_TAB_SWITCHER_ENABLED_KEY = "grid_tab_switcher_enabled";

    /**
     * Whether or not the tab group is enabled.
     * Default value is false.
     */
    public static final String TAB_GROUPS_ANDROID_ENABLED_KEY = "tab_group_android_enabled";

    /**
     * Whether or not the Duet-TabStrip integration is enabled.
     * Default value is false.
     */
    public static final String DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED_KEY =
            "Chrome.Flags.DuetTabstripIntegrationEnabled";

    /**
     * Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task prioritization
     * experiment is enabled). Default value is true.
     */
    public static final String PRIORITIZE_BOOTSTRAP_TASKS_KEY = "prioritize_bootstrap_tasks";

    /**
     * Whether warming up network service is enabled.
     * Default value is false.
     */
    public static final String NETWORK_SERVICE_WARM_UP_ENABLED_KEY =
            "network_service_warm_up_enabled";

    /**
     * Key to cache whether immersive ui mode is enabled.
     */
    public static final String IMMERSIVE_UI_MODE_ENABLED = "immersive_ui_mode_enabled";

    /**
     * Key to cache whether SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT is enabled.
     */
    public static final String SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT =
            "swap_pixel_format_to_fix_convert_from_translucent";

    /**
     * These values are currently used as SharedPreferences keys.
     *
     * @return The list of [keys in use].
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createKeysInUse() {
        // clang-format off
        return Arrays.asList(
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP,
                CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT,
                CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER,
                CONTEXTUAL_SEARCH_OLDEST_WEEK,
                CONTEXTUAL_SEARCH_NEWEST_WEEK,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_0,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_1,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_2,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_3,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_4,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_0,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_1,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_2,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_3,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_4,
                PROMOS_SKIPPED_ON_FIRST_START,
                SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION,
                SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES,
                SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS,
                SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS,
                CHROME_DEFAULT_BROWSER,
                UI_THEME_SETTING_KEY,
                DARKEN_WEBSITES_ENABLED_KEY,
                CONTENT_SUGGESTIONS_SHOWN_KEY,
                SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START,
                CRASH_UPLOAD_SUCCESS_BROWSER,
                CRASH_UPLOAD_SUCCESS_RENDERER,
                CRASH_UPLOAD_SUCCESS_GPU,
                CRASH_UPLOAD_SUCCESS_OTHER,
                CRASH_UPLOAD_FAILURE_BROWSER,
                CRASH_UPLOAD_FAILURE_RENDERER,
                CRASH_UPLOAD_FAILURE_GPU,
                CRASH_UPLOAD_FAILURE_OTHER,
                VERIFIED_DIGITAL_ASSET_LINKS,
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP,
                ACCESSIBILITY_TAB_SWITCHER,
                LATEST_UNSUPPORTED_VERSION,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA,
                WEBAPK_UNINSTALLED_PACKAGES,
                REACHED_CODE_PROFILER_GROUP_KEY,
                OFFLINE_INDICATOR_V2_ENABLED_KEY,
                PREF_PERSONALIZED_SIGNIN_PROMO_DECLINED,
                PREF_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT,

                // Cached feature flags
                SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY,
                SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY,
                INTEREST_FEED_CONTENT_SUGGESTIONS_KEY,
                DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY,
                BOTTOM_TOOLBAR_ENABLED_KEY,
                ADAPTIVE_TOOLBAR_ENABLED_KEY,
                LABELED_BOTTOM_TOOLBAR_ENABLED_KEY,
                NIGHT_MODE_AVAILABLE_KEY,
                NIGHT_MODE_DEFAULT_TO_LIGHT,
                NIGHT_MODE_CCT_AVAILABLE_KEY,
                COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY,
                START_SURFACE_ENABLED_KEY,
                GRID_TAB_SWITCHER_ENABLED_KEY,
                TAB_GROUPS_ANDROID_ENABLED_KEY,
                DUET_TABSTRIP_INTEGRATION_ANDROID_ENABLED_KEY,
                PRIORITIZE_BOOTSTRAP_TASKS_KEY,
                NETWORK_SERVICE_WARM_UP_ENABLED_KEY,
                IMMERSIVE_UI_MODE_ENABLED,
                SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT
        );
        // clang-format on
    }

    /**
     * These values have been used as SharedPreferences keys in the past and should not be reused
     * reused. Do not remove values from this list.
     *
     * @return The list of [deprecated keys].
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createDeprecatedKeysForTesting() {
        // clang-format off
        return Arrays.asList(
                "allow_low_end_device_ui",
                "website_settings_filter",
                "chrome_modern_design_enabled",
                "home_page_button_force_enabled",
                "homepage_tile_enabled",
                "ntp_button_enabled",
                "ntp_button_variant",
                "tab_persistent_store_task_runner_enabled",
                "inflate_toolbar_on_background_thread",
                "sole_integration_enabled",
                "webapk_number_of_uninstalls",
                "allow_starting_service_manager_only",
                "chrome_home_user_enabled",
                "chrome_home_opt_out_snackbar_shown",
                "chrome_home_info_promo_shown",
                "chrome_home_enabled_date",
                "PrefMigrationVersion",
                "click_to_call_open_dialer_directly"
        );
        // clang-format on
    }

    /**
     * Do not add new constants to this list. Instead, declare new keys in the format
     * "Chrome.[Feature].[Key]", for example "Chrome.FooBar.FooEnabled".
     *
     * @return The list of [keys in use] that does not conform to the "Chrome.[Feature].[Key]"
     *     format.
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createGrandfatheredFormatKeysForTesting() {
        // clang-format off
        return Arrays.asList(
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP,
                CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT,
                CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER,
                CONTEXTUAL_SEARCH_OLDEST_WEEK,
                CONTEXTUAL_SEARCH_NEWEST_WEEK,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_0,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_1,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_2,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_3,
                CONTEXTUAL_SEARCH_CLICKS_WEEK_4,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_0,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_1,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_2,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_3,
                CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_4,
                PROMOS_SKIPPED_ON_FIRST_START,
                SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION,
                SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES,
                SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS,
                SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS,
                CHROME_DEFAULT_BROWSER,
                UI_THEME_SETTING_KEY,
                DARKEN_WEBSITES_ENABLED_KEY,
                CONTENT_SUGGESTIONS_SHOWN_KEY,
                SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START,
                CRASH_UPLOAD_SUCCESS_BROWSER,
                CRASH_UPLOAD_SUCCESS_RENDERER,
                CRASH_UPLOAD_SUCCESS_GPU,
                CRASH_UPLOAD_SUCCESS_OTHER,
                CRASH_UPLOAD_FAILURE_BROWSER,
                CRASH_UPLOAD_FAILURE_RENDERER,
                CRASH_UPLOAD_FAILURE_GPU,
                CRASH_UPLOAD_FAILURE_OTHER,
                VERIFIED_DIGITAL_ASSET_LINKS,
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP,
                ACCESSIBILITY_TAB_SWITCHER,
                LATEST_UNSUPPORTED_VERSION,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA,
                WEBAPK_UNINSTALLED_PACKAGES,
                REACHED_CODE_PROFILER_GROUP_KEY,
                OFFLINE_INDICATOR_V2_ENABLED_KEY,
                PREF_PERSONALIZED_SIGNIN_PROMO_DECLINED,
                PREF_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT,

                // Cached feature flags
                SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY,
                SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY,
                INTEREST_FEED_CONTENT_SUGGESTIONS_KEY,
                DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY,
                BOTTOM_TOOLBAR_ENABLED_KEY,
                ADAPTIVE_TOOLBAR_ENABLED_KEY,
                LABELED_BOTTOM_TOOLBAR_ENABLED_KEY,
                NIGHT_MODE_AVAILABLE_KEY,
                NIGHT_MODE_DEFAULT_TO_LIGHT,
                NIGHT_MODE_CCT_AVAILABLE_KEY,
                COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY,
                START_SURFACE_ENABLED_KEY,
                GRID_TAB_SWITCHER_ENABLED_KEY,
                TAB_GROUPS_ANDROID_ENABLED_KEY,
                PRIORITIZE_BOOTSTRAP_TASKS_KEY,
                NETWORK_SERVICE_WARM_UP_ENABLED_KEY,
                IMMERSIVE_UI_MODE_ENABLED,
                SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT
        );
        // clang-format on
    }

    private ChromePreferenceKeys() {}
}

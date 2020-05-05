// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and
 * which variation should be used.
 */
public class StartSurfaceConfiguration {
    public static final StringCachedFieldTrialParameter START_SURFACE_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation", "");
    public static final BooleanCachedFieldTrialParameter START_SURFACE_EXCLUDE_MV_TILES =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "exclude_mv_tiles", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_HIDE_INCOGNITO_SWITCH =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    "hide_switch_when_no_incognito_tabs", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_LAST_ACTIVE_TAB_ONLY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_last_active_tab_only", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_SHOW_STACK_TAB_SWITCHER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_stack_tab_switcher", false);

    /**
     * @return Whether the Start Surface is enabled.
     */
    public static boolean isStartSurfaceEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.START_SURFACE_ANDROID)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        // TODO(crbug.com/1062013): The values cached to START_SURFACE_SINGLE_PANE_ENABLED_KEY
        // should be honored for some time. Remove only after M85 to be safe.
        return isStartSurfaceEnabled()
                && (START_SURFACE_VARIATION.getValue().equals("single")
                        || SharedPreferencesManager.getInstance().readBoolean(
                                ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY, false));
    }
}

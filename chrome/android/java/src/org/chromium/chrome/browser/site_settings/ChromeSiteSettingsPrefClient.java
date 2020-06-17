// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPrefClient;

/**
 * A SiteSettingsPrefClient implementation that delegates to Chrome's PrefServiceBridge.
 */
public class ChromeSiteSettingsPrefClient implements SiteSettingsPrefClient {
    @Override
    public boolean getBlockThirdPartyCookies() {
        return PrefServiceBridge.getInstance().getBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES);
    }

    @Override
    public void setBlockThirdPartyCookies(boolean newValue) {
        PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, newValue);
    }

    @Override
    public boolean isBlockThirdPartyCookiesManaged() {
        return PrefServiceBridge.getInstance().isManagedPreference(Pref.BLOCK_THIRD_PARTY_COOKIES);
    }

    @Override
    public int getCookieControlsMode() {
        return PrefServiceBridge.getInstance().getInteger(Pref.COOKIE_CONTROLS_MODE);
    }

    @Override
    public void setCookieControlsMode(int newValue) {
        PrefServiceBridge.getInstance().setInteger(Pref.COOKIE_CONTROLS_MODE, newValue);
    }

    @Override
    public boolean getEnableQuietNotificationPermissionUi() {
        return PrefServiceBridge.getInstance().getBoolean(
                Pref.ENABLE_QUIET_NOTIFICATION_PERMISSION_UI);
    }

    @Override
    public void setEnableQuietNotificationPermissionUi(boolean newValue) {
        PrefServiceBridge.getInstance().setBoolean(
                Pref.ENABLE_QUIET_NOTIFICATION_PERMISSION_UI, newValue);
    }

    @Override
    public void clearEnableNotificationPermissionUi() {
        PrefServiceBridge.getInstance().clearPref(Pref.ENABLE_QUIET_NOTIFICATION_PERMISSION_UI);
    }

    @Override
    public void setNotificationsVibrateEnabled(boolean newValue) {
        PrefServiceBridge.getInstance().setBoolean(Pref.NOTIFICATIONS_VIBRATE_ENABLED, newValue);
    }
}

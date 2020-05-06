// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

/**
 * An interface that allows the Site Settings UI to read and write native prefs.
 *
 * This interface was introduced because there's currently no way for a component to access native
 * prefs from Java, and should be removed once that functionality exists.
 */
// TODO(crbug.com/1071603): Remove this once PrefServiceBridge is componentized.
public interface SiteSettingsPrefClient {
    boolean getBlockThirdPartyCookies();
    void setBlockThirdPartyCookies(boolean newValue);
    boolean isBlockThirdPartyCookiesManaged();

    int getCookieControlsMode();
    void setCookieControlsMode(int newValue);

    boolean getEnableQuietNotificationPermissionUi();
    void setEnableQuietNotificationPermissionUi(boolean newValue);
    void clearEnableNotificationPermissionUi();

    void setNotificationsVibrateEnabled(boolean newValue);
}

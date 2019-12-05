// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Utility class that interacts with native to retrieve and set permission-related settings.
 */
public class PermissionSettingsBridge {
    public static boolean shouldShowNotificationsPromo() {
        return PermissionSettingsBridgeJni.get().shouldShowNotificationsPromo(getProfile());
    }

    public static void didShowNotificationsPromo() {
        PermissionSettingsBridgeJni.get().didShowNotificationsPromo(getProfile());
    }

    private static Profile getProfile() {
        return Profile.getLastUsedProfile().getOriginalProfile();
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowNotificationsPromo(Profile profile);
        void didShowNotificationsPromo(Profile profile);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.v4.app.NotificationCompat;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides Field Trial support for the permissions field trial
 */
public class PermissionFieldTrial {
    @IntDef({UIFlavor.NONE, UIFlavor.MINI_INFOBAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UIFlavor {
        int NONE = 0;
        int MINI_INFOBAR = 3;
    }

    public static @UIFlavor int uiFlavorToUse() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUIET_NOTIFICATION_PROMPTS)) {
            return UIFlavor.NONE;
        }

        return UIFlavor.MINI_INFOBAR;
    }

    public static @ChannelDefinitions.ChannelId String notificationChannelIdToUse() {
        return ChannelDefinitions.ChannelId.BROWSER;
    }

    public static int notificationPriorityToUse() {
        return NotificationCompat.PRIORITY_LOW;
    }

    public static @NotificationUmaTracker.SystemNotificationType int systemNotificationTypeToUse() {
        return NotificationUmaTracker.SystemNotificationType.UNKNOWN;
    }
}

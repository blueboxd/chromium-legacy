// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

/**
 * Provides common functionality for handling sharing notifications.
 */
public final class SharingNotificationUtil {
    /**
     * Shows a notification with a configuration common to all sharing notifications.
     *
     * @param type The type of notification.
     * @param group The notification group.
     * @param id The notification id.
     * @param contentIntent The notification content intent.
     * @param contentTitle The notification title text.
     * @param contentText The notification content text.
     * @param smallIcon The small notification icon.
     */
    public static void showNotification(@SystemNotificationType int type, String group, int id,
            PendingIntentProvider contentIntent, String contentTitle, String contentText,
            @DrawableRes int smallIcon) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(/*preferCompat=*/true,
                                ChannelDefinitions.ChannelId.SHARING,
                                /*remoteAppPackageName=*/null,
                                new NotificationMetadata(type, group, id))
                        .setContentIntent(contentIntent)
                        .setContentTitle(contentTitle)
                        .setContentText(contentText)
                        .setColor(ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.default_icon_color_blue))
                        .setGroup(group)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(smallIcon)
                        .setAutoCancel(true)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        new NotificationManagerProxyImpl(context).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                type, notification.getNotification());
    }

    private SharingNotificationUtil() {}
}

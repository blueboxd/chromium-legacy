// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

/**
 * Manages ClickToCall related notifications for Android.
 */
public class ClickToCallMessageHandler {
    private static final String EXTRA_PHONE_NUMBER = "ClickToCallMessageHandler.EXTRA_PHONE_NUMBER";

    /**
     * Handles the tapping of a notification by opening the dialer with the
     * phone number specified in the notification.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String phoneNumber = intent.getStringExtra(EXTRA_PHONE_NUMBER);
            final Intent dialIntent;
            if (!TextUtils.isEmpty(phoneNumber)) {
                dialIntent = new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phoneNumber));
            } else {
                dialIntent = new Intent(Intent.ACTION_DIAL);
            }
            dialIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(dialIntent);
            new CachedMetrics.BooleanHistogramSample("Sharing.ClickToCallDialIntent")
                    .record(TextUtils.isEmpty(phoneNumber));
        }
    }

    /**
     * Displays a notification that starts a phone call when clicked.
     *
     * @param phoneNumber The phone number to call when the user taps on the notification.
     */
    @CalledByNative
    private static void showNotification(String phoneNumber) {
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, TapReceiver.class).putExtra(EXTRA_PHONE_NUMBER, phoneNumber),
                PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String text = resources.getString(R.string.click_to_call_notification_text);
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(/*preferCompat=*/true,
                                ChannelDefinitions.ChannelId.SHARING,
                                /*remoteAppPackageName=*/null,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                                        NotificationConstants.GROUP_CLICK_TO_CALL,
                                        NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL))
                        .setContentIntent(contentIntent)
                        .setContentTitle(phoneNumber)
                        .setContentText(text)
                        .setColor(ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.default_icon_color_blue))
                        .setGroup(NotificationConstants.GROUP_CLICK_TO_CALL)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(R.drawable.ic_phone_googblue_36dp)
                        .setAutoCancel(true)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        new NotificationManagerProxyImpl(context).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                notification.getNotification());
    }
}

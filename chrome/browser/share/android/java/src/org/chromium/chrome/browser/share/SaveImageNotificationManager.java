// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.core.app.NotificationCompat;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Manages save image related notifications for Android.
 */
public class SaveImageNotificationManager {
    private static final String TAG = "share";

    private static final String MIME_TYPE = "image/JPEG";
    private static final String EXTRA_INTENT_TYPE =
            "org.chromium.chrome.browser.share.SaveImageNotificationManager.EXTRA_INTENT_TYPE";
    private static final String EXTRA_ID =
            "org.chromium.chrome.browser.share.SaveImageNotificationManager.EXTRA_ID";

    private static int sNextId;

    /**
     * Handles the tapping of a notification by starting an intent to view downloaded content.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int intentType = IntentUtils.safeGetIntExtra(
                    intent, EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.UNKNOWN);
            int id = IntentUtils.safeGetIntExtra(
                    intent, EXTRA_ID, NotificationIntentInterceptor.IntentType.UNKNOWN);
            switch (intentType) {
                case NotificationIntentInterceptor.IntentType.UNKNOWN:
                    break;
                case NotificationIntentInterceptor.IntentType.CONTENT_INTENT:
                    Uri uri = intent.getData();
                    Intent viewIntent = MediaViewerUtils.getMediaViewerIntent(
                            uri, uri, Intent.normalizeMimeType(MIME_TYPE), true);
                    IntentHandler.startActivityForTrustedIntent(viewIntent);
                    close(context, id);
                    RecordUserAction.record("SharingQRCode.SuccessNotificationTapped");
                    break;
                case NotificationIntentInterceptor.IntentType.DELETE_INTENT:
                    RecordUserAction.record("SharingQRCode.FailureNotificationTapped");
                    close(context, id);
            }
        }
    }

    /**
     * Displays a notification for successful download.
     *
     * @param context The Context to use for accessing notification manager.
     * @param uri The Uri of the notification to show.
     * @param notificationTitle The title of the notification.
     */
    public static void showSuccessNotification(Context context, Uri uri, String notificationTitle) {
        String notificationText =
                context.getResources().getString(R.string.download_notification_completed);
        int iconId = R.drawable.offline_pin;
        showNotification(context, uri, notificationTitle, notificationText, iconId,
                NotificationIntentInterceptor.IntentType.CONTENT_INTENT);
    }

    /**
     * Displays a notification for failed download.
     *
     * @param context The Context to use for accessing notification manager.
     * @param uri The Uri of the notification to hide.
     * @param notificationTitle The title of the notification.
     */
    public static void showFailureNotification(Context context, Uri uri, String notificationTitle) {
        String notificationText =
                context.getResources().getString(R.string.download_notification_failed);
        int iconId = android.R.drawable.stat_sys_download_done;
        showNotification(context, uri, notificationTitle, notificationText, iconId,
                NotificationIntentInterceptor.IntentType.DELETE_INTENT);
    }

    /**
     * Displays a notification.
     *
     * @param context The Context to use for accessing notification manager.
     * @param uri The Uri of the notification to hide.
     * @param title The title of the notification.
     * @param text The text of the notification.
     * @param iconId The resource id for icon of the notification.
     * @param intentType The type of the notification.
     */
    private static void showNotification(
            Context context, Uri uri, String title, String text, int iconId, int intentType) {
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context, sNextId++,
                new Intent(context, SaveImageNotificationManager.TapReceiver.class)
                        .setData(uri)
                        .putExtra(EXTRA_INTENT_TYPE, intentType)
                        .putExtra(EXTRA_ID, sNextId),
                PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);

        // Build the notification.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true,
                                ChromeChannelDefinitions.ChannelId.SHARING, null,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .SHARE_SAVE_IMAGE,
                                        NotificationConstants.GROUP_SHARE_SAVE_IMAGE, sNextId))
                        .setContentIntent(contentIntent)
                        .setContentTitle(title)
                        .setContentText(text)
                        .setGroup(NotificationConstants.GROUP_SHARE_SAVE_IMAGE)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(iconId)
                        .setDefaults(Notification.DEFAULT_ALL);
        manager.notify(builder.buildChromeNotification());
    }

    /**
     * Closes a notification.
     *
     * @param context The Context to use for accessing notification manager.
     * @param guid The GUID of the notification to hide.
     */
    private static void close(Context context, int id) {
        new NotificationManagerProxyImpl(context).cancel(
                NotificationConstants.GROUP_SHARE_SAVE_IMAGE, id);
    }
}

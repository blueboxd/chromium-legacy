// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;

/**
 * Handles Shared Clipboard messages and notifications for Android.
 */
public class SharedClipboardMessageHandler {
    /**
     * Handles the tapping of a notification.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // TODO(mvanouwerkerk): handle this.
        }
    }

    /**
     * Displays a notification to tell the user that new clipboard contents have been received and
     * written to the clipboard.
     */
    @CalledByNative
    private static void showNotification(String deviceName) {
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0, new Intent(context, TapReceiver.class),
                PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String notificationTitle = TextUtils.isEmpty(deviceName)
                ? resources.getString(R.string.shared_clipboard_notification_title_unknown_device)
                : resources.getString(R.string.shared_clipboard_notification_title, deviceName);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD, contentIntent,
                notificationTitle, resources.getString(R.string.shared_clipboard_notification_text),
                R.drawable.ic_devices_16dp, R.drawable.shared_clipboard_40dp);
    }
}

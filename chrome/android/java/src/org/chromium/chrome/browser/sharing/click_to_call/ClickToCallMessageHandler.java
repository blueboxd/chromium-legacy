// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;

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
            context.startActivity(dialIntent);
            ClickToCallUma.recordDialerShown(TextUtils.isEmpty(phoneNumber));
        }
    }

    /**
     * Displays a notification that starts a phone call when clicked.
     *
     * @param phoneNumber The phone number to call when the user taps on the notification.
     */
    @CalledByNative
    private static void showNotification(String phoneNumber) {
        ClickToCallUma.recordMessageReceived();
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, TapReceiver.class).putExtra(EXTRA_PHONE_NUMBER, phoneNumber),
                PendingIntent.FLAG_UPDATE_CURRENT);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                NotificationConstants.GROUP_CLICK_TO_CALL,
                NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL, contentIntent, phoneNumber,
                context.getResources().getString(R.string.click_to_call_notification_text),
                R.drawable.ic_phone_googblue_36dp);
    }
}

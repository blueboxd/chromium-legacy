// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Used by notification scheduler to display the notification in Android UI.
 */
public class DisplayAgent {
    private static final String TAG = "DisplayAgent";
    private static final String DISPLAY_AGENT_TAG = "NotificationSchedulerDisplayAgent";
    private static final int DISPLAY_AGENT_NOTIFICATION_ID = 0;

    private static final String EXTRA_INTENT_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_INTENT_TYPE";
    private static final String EXTRA_GUID =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_GUID";
    /**
     * Contains all data needed to build Android notification for notification scheduler.
     * TODO(xingliu): Split this to data used for UI and exposed to clients, and system level data
     * used internally, see https://crbug.com/977255.
     */
    private static class DisplayData {
        public final String guid;
        public final String title;
        public final String message;
        public final Bitmap icon;

        @CalledByNative
        public DisplayData(String guid, String title, String message, Bitmap icon) {
            this.guid = guid;
            this.title = title;
            this.message = message;
            this.icon = icon;
        }
    }

    /**
     * Receives notification events from Android, like clicks, dismiss, etc.
     */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserParts parts = new EmptyBrowserParts() {
                @Override
                public void finishNativeInitialization() {
                    handleUserAction(intent);
                }
            };

            // Try to load native.
            try {
                ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
                ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
            } catch (ProcessInitException e) {
                Log.e(TAG, "Unable to load native library.", e);
                ChromeApplication.reportStartupErrorAndExit(e);
            }
        }
    }

    private static void handleUserAction(Intent intent) {
        @NotificationIntentInterceptor.IntentType
        int intentType = IntentUtils.safeGetIntExtra(
                intent, EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.UNKNOWN);
        String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_GUID);
        switch (intentType) {
            case NotificationIntentInterceptor.IntentType.UNKNOWN:
                break;
            case NotificationIntentInterceptor.IntentType.CONTENT_INTENT:
                nativeOnContentClick(Profile.getLastUsedProfile(), guid);
                break;
            case NotificationIntentInterceptor.IntentType.DELETE_INTENT:
                nativeOnDismiss(Profile.getLastUsedProfile(), guid);
                break;
            case NotificationIntentInterceptor.IntentType.ACTION_INTENT:
                nativeOnActionButton(Profile.getLastUsedProfile(), guid);
                break;
        }
    }

    /**
     * Contains Android platform specific data to construct a notification.
     */
    private static class AndroidNotificationData {
        public final @ChannelId String channel;
        public final @SystemNotificationType int systemNotificationType;
        public AndroidNotificationData(String channel, int systemNotificationType) {
            this.channel = channel;
            this.systemNotificationType = systemNotificationType;
        }
    }

    private static AndroidNotificationData toAndroidNotificationData(DisplayData data) {
        return new AndroidNotificationData(ChannelId.BROWSER, SystemNotificationType.UNKNOWN);
    }

    private static Intent buildIntent(Context context,
            @NotificationIntentInterceptor.IntentType int intentType, String guid) {
        Intent intent = new Intent(context, DisplayAgent.Receiver.class);
        intent.putExtra(EXTRA_INTENT_TYPE, intentType);
        intent.putExtra(EXTRA_GUID, guid);
        return intent;
    }

    @CalledByNative
    private static void showNotification(DisplayData displayData) {
        AndroidNotificationData platformData = toAndroidNotificationData(displayData);
        // TODO(xingliu): Plumb platform specific data from native. Support single notification
        // mode and provide correct notification id. Support buttons.
        Context context = ContextUtils.getApplicationContext();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true /* preferCompat */,
                        platformData.channel, null /* remoteAppPackageName */,
                        new NotificationMetadata(platformData.systemNotificationType,
                                DISPLAY_AGENT_TAG, DISPLAY_AGENT_NOTIFICATION_ID));
        builder.setContentTitle(displayData.title);
        builder.setContentText(displayData.message);

        // TODO(xingliu): Use the icon from native. Support big icon.
        builder.setSmallIcon(R.drawable.ic_chrome);

        // Default content click behavior.
        Intent contentIntent = buildIntent(
                context, NotificationIntentInterceptor.IntentType.CONTENT_INTENT, displayData.guid);
        builder.setContentIntent(PendingIntentProvider.getBroadcast(
                context, 0, contentIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // Default dismiss behavior.
        Intent dismissIntent = buildIntent(
                context, NotificationIntentInterceptor.IntentType.DELETE_INTENT, displayData.guid);
        builder.setDeleteIntent(PendingIntentProvider.getBroadcast(
                context, 0, dismissIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        ChromeNotification notification = builder.buildChromeNotification();
        new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                platformData.systemNotificationType, notification.getNotification());
    }

    private DisplayAgent() {}

    private static native void nativeOnContentClick(Profile profile, String guid);
    private static native void nativeOnDismiss(Profile profile, String guid);
    private static native void nativeOnActionButton(Profile profile, String guid);
}

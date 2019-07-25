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

import java.util.ArrayList;

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
    private static final String EXTRA_ACTION_BUTTON_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_ACTION_BUTTON_TYPE";
    private static final String EXTRA_SCHEDULER_CLIENT_TYPE =
            "org.chromium.chrome.browser.notifications.scheduler.EXTRA_SCHEDULER_CLIENT_TYPE ";

    /**
     * Contains button info on the notification.
     */
    private static class Button {
        public final String text;
        public final @ActionButtonType int type;
        public final String id;

        public Button(String text, @ActionButtonType int type, String id) {
            this.text = text;
            this.type = type;
            this.id = id;
        }
    }

    /**
     * Contains all data needed to build Android notification in the UI, specified by the client.
     */
    private static class NotificationData {
        public final String title;
        public final String message;
        public final Bitmap icon;
        public ArrayList<Button> buttons = new ArrayList<>();

        private NotificationData(String title, String message, Bitmap icon) {
            // TODO(xingliu): Populate custom data.
            this.title = title;
            this.message = message;
            this.icon = icon;
        }
    }

    @CalledByNative
    private static void addButton(
            NotificationData notificationData, String text, @ActionButtonType int type, String id) {
        notificationData.buttons.add(new Button(text, type, id));
    }

    @CalledByNative
    private static NotificationData buildNotificationData(
            String title, String message, Bitmap icon) {
        return new NotificationData(title, message, icon);
    }

    /**
     * Contains data used used by the notification scheduling system internally to build the
     * notification.
     */
    private static class SystemData {
        public @SchedulerClientType int type;
        public final String guid;

        public SystemData(@SchedulerClientType int type, String guid) {
            this.type = type;
            this.guid = guid;
        }
    }

    @CalledByNative
    private static SystemData buildSystemData(@SchedulerClientType int type, String guid) {
        return new SystemData(type, guid);
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
        @SchedulerClientType
        int clientType = IntentUtils.safeGetIntExtra(
                intent, EXTRA_SCHEDULER_CLIENT_TYPE, SchedulerClientType.UNKNOWN);
        switch (intentType) {
            case NotificationIntentInterceptor.IntentType.UNKNOWN:
                break;
            case NotificationIntentInterceptor.IntentType.CONTENT_INTENT:
                nativeOnContentClick(Profile.getLastUsedProfile(), clientType, guid);
                break;
            case NotificationIntentInterceptor.IntentType.DELETE_INTENT:
                nativeOnDismiss(Profile.getLastUsedProfile(), clientType, guid);
                break;
            case NotificationIntentInterceptor.IntentType.ACTION_INTENT:
                int actionButtonType = IntentUtils.safeGetIntExtra(
                        intent, EXTRA_ACTION_BUTTON_TYPE, ActionButtonType.UNKNOWN_ACTION);
                nativeOnActionButton(
                        Profile.getLastUsedProfile(), clientType, guid, actionButtonType);
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

    private static AndroidNotificationData toAndroidNotificationData(NotificationData data) {
        return new AndroidNotificationData(ChannelId.BROWSER, SystemNotificationType.UNKNOWN);
    }

    private static Intent buildIntent(Context context,
            @NotificationIntentInterceptor.IntentType int intentType, SystemData systemData) {
        Intent intent = new Intent(context, DisplayAgent.Receiver.class);
        intent.putExtra(EXTRA_INTENT_TYPE, intentType);
        intent.putExtra(EXTRA_SCHEDULER_CLIENT_TYPE, systemData.type);
        intent.putExtra(EXTRA_GUID, systemData.guid);
        return intent;
    }

    @CalledByNative
    private static void showNotification(NotificationData notificationData, SystemData systemData) {
        AndroidNotificationData platformData = toAndroidNotificationData(notificationData);
        // TODO(xingliu): Plumb platform specific data from native.
        // mode and provide correct notification id. Support buttons.
        Context context = ContextUtils.getApplicationContext();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true /* preferCompat */,
                        platformData.channel, null /* remoteAppPackageName */,
                        new NotificationMetadata(platformData.systemNotificationType,
                                DISPLAY_AGENT_TAG, systemData.guid.hashCode()));
        builder.setContentTitle(notificationData.title);
        builder.setContentText(notificationData.message);

        // TODO(xingliu): Use the icon from native. Support big icon.
        builder.setSmallIcon(R.drawable.ic_chrome);

        // Default content click behavior.
        Intent contentIntent = buildIntent(
                context, NotificationIntentInterceptor.IntentType.CONTENT_INTENT, systemData);
        builder.setContentIntent(PendingIntentProvider.getBroadcast(context,
                getRequestCode(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT, systemData.guid),
                contentIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // Default dismiss behavior.
        Intent dismissIntent = buildIntent(
                context, NotificationIntentInterceptor.IntentType.DELETE_INTENT, systemData);
        builder.setDeleteIntent(PendingIntentProvider.getBroadcast(context,
                getRequestCode(
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT, systemData.guid),
                dismissIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // Add the buttons.
        for (int i = 0; i < notificationData.buttons.size(); i++) {
            Button button = notificationData.buttons.get(i);
            Intent actionIntent = buildIntent(
                    context, NotificationIntentInterceptor.IntentType.ACTION_INTENT, systemData);
            actionIntent.putExtra(EXTRA_ACTION_BUTTON_TYPE, button.type);

            // TODO(xingliu): Support button icon. See https://crbug.com/983354
            builder.addAction(0 /*icon_id*/, button.text,
                    PendingIntentProvider.getBroadcast(context,
                            getRequestCode(NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                                    systemData.guid),
                            actionIntent, PendingIntent.FLAG_UPDATE_CURRENT),
                    NotificationUmaTracker.ActionType.UNKNOWN);
        }

        ChromeNotification notification = builder.buildChromeNotification();
        new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                platformData.systemNotificationType, notification.getNotification());
    }

    /**
     * Returns the request code for a specific intent. Android will not distinguish intents based on
     * extra data. Different intent must have different request code.
     */
    private static int getRequestCode(
            @NotificationIntentInterceptor.IntentType int intentType, String guid) {
        int hash = guid.hashCode();
        hash += 31 * hash + intentType;
        return hash;
    }

    private DisplayAgent() {}

    private static native void nativeOnContentClick(
            Profile profile, @SchedulerClientType int type, String guid);
    private static native void nativeOnDismiss(
            Profile profile, @SchedulerClientType int type, String guid);
    private static native void nativeOnActionButton(Profile profile,
            @SchedulerClientType int clientType, String guid, @ActionButtonType int type);
}

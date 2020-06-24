// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.Intent;
import android.support.v4.media.session.MediaSessionCompat;

import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.browser_ui.notifications.ChromeNotification;
import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;

/**
 * A glue class for MediaSession.
 * This class defines delegates that provide WebLayer-specific behavior to shared MediaSession code.
 * It also manages the lifetime of {@link MediaNotificationController} and the {@link Service}
 * associated with the notification.
 */
class MediaSessionManager {
    // This is a singleton because there's only at most one MediaSession active at a time.
    @SuppressLint("StaticFieldLeak")
    static MediaNotificationController sController;

    private static int sNotificationId = 0;

    static void serviceStarted(Service service, Intent intent) {
        if (sController != null && sController.processIntent(service, intent)) return;

        // The service has been started with startForegroundService() but the
        // notification hasn't been shown. See similar logic in {@link
        // ChromeMediaNotificationControllerDelegate}.
        MediaNotificationController.finishStartingForegroundServiceOnO(
                service, createChromeNotificationBuilder().buildChromeNotification());
        // Call stopForeground to guarantee Android unset the foreground bit.
        ForegroundServiceUtils.getInstance().stopForeground(
                service, Service.STOP_FOREGROUND_REMOVE);
        service.stopSelf();
    }

    static void serviceDestroyed() {
        if (sController != null) sController.onServiceDestroyed();
        sController = null;
    }

    static MediaSessionHelper.Delegate createMediaSessionHelperDelegate(int tabId) {
        return new MediaSessionHelper.Delegate() {
            @Override
            public Intent createBringTabToFrontIntent() {
                return IntentUtils.createBringTabToFrontIntent(tabId);
            }

            @Override
            public boolean fetchLargeFaviconImage() {
                // TODO(crbug.com/1076463): WebLayer doesn't support favicons.
                return false;
            }

            @Override
            public MediaNotificationInfo.Builder createMediaNotificationInfoBuilder() {
                ensureNotificationId();
                return new MediaNotificationInfo.Builder().setInstanceId(tabId).setId(
                        sNotificationId);
            }

            @Override
            public void showMediaNotification(MediaNotificationInfo notificationInfo) {
                assert notificationInfo.id == sNotificationId;
                if (sController == null) {
                    sController = new MediaNotificationController(
                            new WebLayerMediaNotificationControllerDelegate());
                }
                sController.mThrottler.queueNotification(notificationInfo);
            }

            @Override
            public void hideMediaNotification() {
                if (sController != null) sController.hideNotification(tabId);
            }

            @Override
            public void activateAndroidMediaSession() {
                if (sController != null) sController.activateAndroidMediaSession(tabId);
            }
        };
    }

    private static class WebLayerMediaNotificationControllerDelegate
            implements MediaNotificationController.Delegate {
        @Override
        public Intent createServiceIntent() {
            return WebLayerImpl.createMediaSessionServiceIntent();
        }

        @Override
        public String getAppName() {
            return WebLayerImpl.getClientApplicationName();
        }

        @Override
        public String getNotificationGroupName() {
            return "org.chromium.weblayer.MediaSession";
        }

        @Override
        public ChromeNotificationBuilder createChromeNotificationBuilder() {
            return MediaSessionManager.createChromeNotificationBuilder();
        }

        @Override
        public void onMediaSessionUpdated(MediaSessionCompat session) {
            // This is only relevant when casting.
        }

        @Override
        public void logNotificationShown(ChromeNotification notification) {}
    }

    private static ChromeNotificationBuilder createChromeNotificationBuilder() {
        ensureNotificationId();

        // Only the null tag will work as expected, because {@link Service#startForeground()} only
        // takes an ID and no tag. If we pass a tag here, then the notification that's used to
        // display a paused state (no foreground service) will not be identified as the same one
        // that's used with the foreground service.
        return WebLayerNotificationBuilder.create(
                WebLayerNotificationChannels.ChannelId.MEDIA_PLAYBACK,
                new NotificationMetadata(0, null /*notificationTag*/, sNotificationId));
    }

    private static void ensureNotificationId() {
        if (sNotificationId == 0) sNotificationId = WebLayerImpl.getMediaSessionNotificationId();
    }
}

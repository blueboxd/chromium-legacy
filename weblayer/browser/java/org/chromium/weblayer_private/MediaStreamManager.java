// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * A per-tab object that manages notifications for ongoing media streams (microphone/camera). This
 * object is created by {@link TabImpl} and creates and destroys its native equivalent.
 *
 * TODO(estade): remove notifications that have persisted across restarts (due to the app crashing).
 */
@JNINamespace("weblayer")
public class MediaStreamManager {
    private static boolean sCreatedChannel = false;
    private static final String WEBRTC_PREFIX = "org.chromium.weblayer.webrtc";
    private static final String CHANNEL_ID = WEBRTC_PREFIX + ".channel";
    private static final String EXTRA_TAB_ID = WEBRTC_PREFIX + ".TAB_ID";
    private static final String ACTIVATE_TAB_INTENT = WEBRTC_PREFIX + ".ACTIVATE_TAB";
    private static final String AV_STREAM_TAG = WEBRTC_PREFIX + ".avstream";

    // The notification ID matches the tab ID, which uniquely identifies the notification when
    // paired with the tag.
    private int mNotificationId;

    // Pointer to the native MediaStreamManager.
    private long mNative;

    /**
     * @return a string that prefixes all intents that can be handled by {@link forwardIntent}.
     */
    public static String getIntentPrefix() {
        return WEBRTC_PREFIX;
    }

    /**
     * Handles an intent coming from a media streaming notification.
     * @param intent the intent which was previously posted via {@link update}.
     */
    public static void forwardIntent(Intent intent) {
        assert intent.getAction().equals(ACTIVATE_TAB_INTENT);
        int tabId = intent.getIntExtra(EXTRA_TAB_ID, -1);
        TabImpl tab = TabImpl.getTabById(tabId);
        if (tab == null) return;

        try {
            tab.getClient().bringTabToFront();
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    public MediaStreamManager(TabImpl tab) {
        mNotificationId = tab.getId();
        mNative = MediaStreamManagerJni.get().create(this, tab.getWebContents());
    }

    public void destroy() {
        NotificationManagerCompat notificationManager = getNotificationManager();
        if (notificationManager == null) return;
        notificationManager.cancel(AV_STREAM_TAG, mNotificationId);

        MediaStreamManagerJni.get().destroy(mNative);
    }

    /**
     * Called after the tab's media streaming state has changed.
     *
     * A notification should be shown (or updated) iff one of the parameters is true, otherwise any
     * existing notification will be removed.
     *
     * @param audio true if the tab is streaming audio.
     * @param video true if the tab is streaming video.
     */
    @CalledByNative
    private void update(boolean audio, boolean video) {
        // The notification intent is not handled in the client prior to M84.
        if (WebLayerFactoryImpl.getClientMajorVersion() < 84) return;

        NotificationManagerCompat notificationManager = getNotificationManager();
        if (notificationManager == null) return;
        createNotificationChannel();

        if (!audio && !video) {
            notificationManager.cancel(AV_STREAM_TAG, mNotificationId);
            return;
        }

        Intent intent = WebLayerImpl.createIntent();
        intent.putExtra(EXTRA_TAB_ID, mNotificationId);
        intent.setAction(ACTIVATE_TAB_INTENT);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                ContextUtils.getApplicationContext(), mNotificationId, intent, 0);
        // TODO(estade): use localized text and correct icon.
        NotificationCompat.Builder builder =
                new NotificationCompat.Builder(ContextUtils.getApplicationContext(), CHANNEL_ID)
                        .setOngoing(true)
                        .setLocalOnly(true)
                        .setAutoCancel(false)
                        .setContentIntent(pendingIntent)
                        .setSmallIcon(android.R.drawable.ic_menu_camera)
                        .setContentTitle(audio && video
                                        ? "all the streamz"
                                        : audio ? "audio streamz" : "video streamz");
        notificationManager.notify(AV_STREAM_TAG, mNotificationId, builder.build());
    }

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (!sCreatedChannel && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // TODO(estade): use localized channel name.
            ContextUtils.getApplicationContext()
                    .getSystemService(NotificationManager.class)
                    .createNotificationChannel(new NotificationChannel(
                            CHANNEL_ID, "WebRTC", NotificationManager.IMPORTANCE_LOW));
        }

        sCreatedChannel = true;
    }

    private NotificationManagerCompat getNotificationManager() {
        if (ContextUtils.getApplicationContext() == null) {
            return null;
        }
        return NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    @NativeMethods
    interface Natives {
        long create(MediaStreamManager caller, WebContents webContents);
        void destroy(long manager);
    }
}

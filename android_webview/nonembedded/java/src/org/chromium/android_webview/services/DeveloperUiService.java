// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;

import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.services.IDeveloperUiService;

import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * A Service to support Developer UI features. This service enables communication between the
 * WebView implementation embedded in apps on the system and the Developer UI.
 */
public final class DeveloperUiService extends Service {
    private static final String CHANNEL_ID = "DevUiChannel";
    private static final int FLAG_OVERRIDE_NOTIFICATION_ID = 1;

    private static final Object sLock = new Object();
    @GuardedBy("sLock")
    private static Map<String, Boolean> sOverriddenFlags = new HashMap<>();

    @GuardedBy("sLock")
    private boolean mDeveloperModeEnabled;

    private final IDeveloperUiService.Stub mBinder = new IDeveloperUiService.Stub() {
        @Override
        public void setFlagOverrides(Map overriddenFlags) {
            if (Binder.getCallingUid() != Process.myUid()) {
                throw new SecurityException(
                        "setFlagOverrides() may only be called by the Developer UI app");
            }
            synchronized (sLock) {
                sOverriddenFlags = overriddenFlags;
                if (sOverriddenFlags.isEmpty()) {
                    disableDeveloperMode();
                } else {
                    enableDeveloperMode();
                }
            }
        }
    };

    public static Map<String, Boolean> getFlagOverrides() {
        synchronized (sLock) {
            // Create a copy so the caller can do what it wants with the Map without worrying about
            // thread safety.
            return new HashMap<>(sOverriddenFlags);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @TargetApi(Build.VERSION_CODES.O)
    private Notification.Builder createNotificationBuilder() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return new Notification.Builder(this, CHANNEL_ID);
        }
        return new Notification.Builder(this);
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void registerDefaultNotificationChannel() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        CharSequence name = "WebView DevTools alerts";
        // The channel importance should be consistent with the Notification priority on pre-O.
        NotificationChannel channel =
                new NotificationChannel(CHANNEL_ID, name, NotificationManager.IMPORTANCE_LOW);
        channel.enableVibration(false);
        channel.enableLights(false);
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(channel);
    }

    private void markAsForegroundService() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            registerDefaultNotificationChannel();
        }

        Intent notificationIntent = new Intent();
        notificationIntent.setClassName(
                getPackageName(), "org.chromium.android_webview.devui.FlagsActivity");
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);

        Notification.Builder builder =
                createNotificationBuilder()
                        .setContentTitle("WARNING: experimental WebView features enabled")
                        .setContentText("Tap to see experimental features.")
                        .setSmallIcon(android.R.drawable.stat_notify_error)
                        .setContentIntent(pendingIntent)
                        .setOngoing(true)
                        .setVisibility(Notification.VISIBILITY_PUBLIC)
                        .setTicker("Experimental WebView features enabled");

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            builder = builder
                              // No sound, vibration, or lights.
                              .setDefaults(0)
                              // This should be consistent with NotificationChannel importance.
                              .setPriority(Notification.PRIORITY_LOW);
        }
        Notification notification = builder.build();

        startForeground(FLAG_OVERRIDE_NOTIFICATION_ID, notification);
    }

    private void enableDeveloperMode() {
        synchronized (sLock) {
            if (mDeveloperModeEnabled) return;
            // Keep this service alive as long as we're in developer mode.
            startService(new Intent(this, DeveloperUiService.class));
            markAsForegroundService();

            ComponentName developerModeState =
                    new ComponentName(this, DeveloperModeUtils.DEVELOPER_MODE_STATE_COMPONENT);
            getPackageManager().setComponentEnabledSetting(developerModeState,
                    PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);

            mDeveloperModeEnabled = true;
        }
    }

    private void disableDeveloperMode() {
        synchronized (sLock) {
            if (!mDeveloperModeEnabled) return;
            mDeveloperModeEnabled = false;

            ComponentName developerModeState =
                    new ComponentName(this, DeveloperModeUtils.DEVELOPER_MODE_STATE_COMPONENT);
            getPackageManager().setComponentEnabledSetting(developerModeState,
                    PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, PackageManager.DONT_KILL_APP);

            // Finally, stop the service explicitly. Do this last to make sure we do the other
            // necessary cleanup.
            stopForeground(/* removeNotification */ true);
            stopSelf();
        }
    }
}

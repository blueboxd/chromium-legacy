// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import com.google.android.gms.gcm.GcmListenerService;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidGcmController;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.components.gcm_driver.LazySubscriptionsManager;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Receives Downstream messages and status of upstream messages from GCM.
 */
public class ChromeGcmListenerService extends GcmListenerService {
    private static final String TAG = "ChromeGcmListener";
    private static final String SHARING_APP_ID = "com.google.chrome.sharing.fcm";

    @Override
    public void onCreate() {
        ProcessInitializationHandler.getInstance().initializePreNative();
        super.onCreate();
    }

    @Override
    public void onMessageReceived(final String from, final Bundle data) {
        boolean hasCollapseKey = !TextUtils.isEmpty(data.getString("collapse_key"));
        GcmUma.recordDataMessageReceived(ContextUtils.getApplicationContext(), hasCollapseKey);

        String invalidationSenderId = AndroidGcmController.get(this).getSenderId();
        if (from.equals(invalidationSenderId)) {
            AndroidGcmController.get(this).onMessageReceived(data);
            return;
        }

        // Dispatch the message to the GCM Driver for native features.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            GCMMessage message = null;
            try {
                message = new GCMMessage(from, data);
            } catch (IllegalArgumentException e) {
                Log.e(TAG, "Received an invalid GCM Message", e);
                return;
            }

            scheduleOrDispatchMessageToDriver(message);
        });
    }

    @Override
    public void onMessageSent(String msgId) {
        Log.d(TAG, "Message sent successfully. Message id: " + msgId);
        GcmUma.recordGcmUpstreamHistogram(
                ContextUtils.getApplicationContext(), GcmUma.UMA_UPSTREAM_SUCCESS);
    }

    @Override
    public void onSendError(String msgId, String error) {
        Log.w(TAG, "Error in sending message. Message id: " + msgId + " Error: " + error);
        GcmUma.recordGcmUpstreamHistogram(
                ContextUtils.getApplicationContext(), GcmUma.UMA_UPSTREAM_SEND_FAILED);
    }

    @Override
    public void onDeletedMessages() {
        // TODO(johnme): Ask GCM to include the subtype in this event.
        Log.w(TAG, "Push messages were deleted, but we can't tell the Service Worker as we don't"
                + "know what subtype (app ID) it occurred for.");
        GcmUma.recordDeletedMessages(ContextUtils.getApplicationContext());
    }

    /**
     * Returns if we deliver the GCMMessage with a background service by calling
     * Context#startService. This will only work if Android has put us in a whitelist to allow
     * background services to be started.
     */
    private static boolean maybeBypassScheduler(GCMMessage message) {
        // Android only puts us on a whitelist for high priority messages.
        if (message.getOriginalPriority() != GCMMessage.Priority.HIGH
                || !SHARING_APP_ID.equals(message.getAppId())) {
            return false;
        }

        // Receiving a high priority push message should put us in a whitelist to start
        // background services.
        try {
            Context context = ContextUtils.getApplicationContext();
            Intent intent = new Intent(context, GCMBackgroundService.class);
            intent.putExtras(message.toBundle());
            context.startService(intent);
            return true;
        } catch (IllegalStateException e) {
            // Failed to start service, maybe we're not whitelisted? Fallback to using
            // BackgroundTaskScheduler to start Chrome.
            // TODO(knollr): Add metrics for this.
            Log.e(TAG, "Could not start background service", e);
            return false;
        }
    }

    /**
     * If Chrome is backgrounded, messages coming from lazy subscriptions are
     * persisted on disk and replayed next time Chrome is forgrounded. If Chrome is forgrounded or
     * if the message isn't coming from a lazy subscription, this method either schedules |message|
     * to be dispatched through the Job Scheduler, which we use on Android N and beyond, or
     * immediately dispatches the message on other versions of Android. Must be called on the UI
     * thread both for the BackgroundTaskScheduler and for dispatching the |message| to the
     * GCMDriver.
     */
    static void scheduleOrDispatchMessageToDriver(GCMMessage message) {
        ThreadUtils.assertOnUiThread();
        final String subscriptionId = LazySubscriptionsManager.buildSubscriptionUniqueId(
                message.getAppId(), message.getSenderId());
        if (!ApplicationStatus.hasVisibleActivities()) {
            boolean isSubscriptionLazy = false;
            long time = SystemClock.elapsedRealtime();
            // TODO(crbug.com/945402): Add metrics for the new high priority message logic.
            if (LazySubscriptionsManager.isSubscriptionLazy(subscriptionId)
                    && message.getOriginalPriority() != GCMMessage.Priority.HIGH) {
                isSubscriptionLazy = true;
                LazySubscriptionsManager.persistMessage(subscriptionId, message);
            }

            // Use {@link CachedMetrics} so this gets reported when native is
            // loaded instead of calling native right away.
            new CachedMetrics.TimesHistogramSample("PushMessaging.TimeToCheckIfSubscriptionLazy")
                    .record(SystemClock.elapsedRealtime() - time);

            if (isSubscriptionLazy) {
                return;
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            if (maybeBypassScheduler(message)) {
                // We've bypassed the scheduler, so nothing to do here.
                return;
            }

            Bundle extras = message.toBundle();

            // TODO(peter): Add UMA for measuring latency introduced by the BackgroundTaskScheduler.
            TaskInfo backgroundTask = TaskInfo.createOneOffTask(TaskIds.GCM_BACKGROUND_TASK_JOB_ID,
                                                      GCMBackgroundTask.class, 0 /* immediately */)
                                              .setExtras(extras)
                                              .build();

            BackgroundTaskSchedulerFactory.getScheduler().schedule(
                    ContextUtils.getApplicationContext(), backgroundTask);

        } else {
            dispatchMessageToDriver(ContextUtils.getApplicationContext(), message);
        }
    }

    /**
     * To be called when a GCM message is ready to be dispatched. Will initialise the native code
     * of the browser process, and forward the message to the GCM Driver. Must be called on the UI
     * thread.
     */
    static void dispatchMessageToDriver(Context applicationContext, GCMMessage message) {
        ThreadUtils.assertOnUiThread();

        try {
            ChromeBrowserInitializer.getInstance(applicationContext).handleSynchronousStartup();
            GCMDriver.dispatchMessage(message);

        } catch (ProcessInitException e) {
            Log.e(TAG, "ProcessInitException while starting the browser process");

            // Since the library failed to initialize nothing in the application can work, so kill
            // the whole application as opposed to just this service.
            System.exit(-1);
        }
    }
}

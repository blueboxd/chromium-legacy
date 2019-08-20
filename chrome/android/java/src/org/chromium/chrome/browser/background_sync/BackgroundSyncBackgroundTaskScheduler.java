// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import android.os.Bundle;
import android.support.annotation.IntDef;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * The {@link BackgroundSyncBackgroundTaskScheduler} singleton is responsible
 * for scheduling and cancelling background tasks to wake Chrome up so that
 * Background Sync events ready to be fired can be fired.
 *
 * Thread model: This class is to be run on the UI thread only.
 */
public class BackgroundSyncBackgroundTaskScheduler {
    /**
     * Denotes the one-off Background Sync Background Tasks scheduled through
     * this class.
     * ONE_SHOT_SYNC_CHROME_WAKE_UP is the task that processes one-shot
     * Background Sync registrations.
     * PERIODIC_SYNC_CHROME_WAKE_UP processes Periodic Background Sync
     * registrations.
     */
    @IntDef({BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
            BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP})
    public @interface BackgroundSyncTask {
        int ONE_SHOT_SYNC_CHROME_WAKE_UP = 0;
        int PERIODIC_SYNC_CHROME_WAKE_UP = 1;
    };

    // Keep in sync with the default min_sync_recovery_time of
    // BackgroundSyncParameters.
    private static final long MIN_SYNC_RECOVERY_TIME = DateUtils.MINUTE_IN_MILLIS * 6;

    // Bundle key for the timestamp of the soonest wakeup time expected for
    // this task.
    public static final String SOONEST_EXPECTED_WAKETIME = "SoonestWakeupTime";

    private static class LazyHolder {
        static final BackgroundSyncBackgroundTaskScheduler INSTANCE =
                new BackgroundSyncBackgroundTaskScheduler();
    }

    @CalledByNative
    public static BackgroundSyncBackgroundTaskScheduler getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Returns the appropriate TaskID to use based on the class of the Background
     * Sync task we're working with.
     *
     * @param taskType The Background Sync task to get the TaskID for.
     */
    @VisibleForTesting
    public static int getAppropriateTaskId(@BackgroundSyncTask int taskType) {
        switch (taskType) {
            case BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP:
                return TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;
            case BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP:
                return TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID;
            default:
                assert false : "Incorrect Background Sync task type";
                return -1;
        }
    }

    /**
     * Returns the appropriate task class to use based on the Background
     * Sync task we're working with.
     *
     * @param taskType The Background Sync task to get the task class for.
     */
    @VisibleForTesting
    public static Class<? extends NativeBackgroundTask> getAppropriateTaskClass(
            @BackgroundSyncTask int taskType) {
        switch (taskType) {
            case BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP:
                return BackgroundSyncBackgroundTask.class;
            case BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP:
                return PeriodicBackgroundSyncChromeWakeUpTask.class;
            default:
                assert false : "Incorrect Background Sync task type";
                return null;
        }
    }

    /**
     * Cancels a Background Sync one-off task, if there's one scheduled.
     *
     * @param taskType The Background Sync task to cancel.
     */
    @VisibleForTesting
    protected void cancelOneOffTask(@BackgroundSyncTask int taskType) {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), getAppropriateTaskId(taskType));
    }

    /**
     * Schedules a one-off background task to wake the browser up on network
     * connectivity and call into native code to fire ready periodic Background Sync
     * events.
     *
     * @param minDelayMs The minimum time to wait before waking the browser.
     * @param taskType   The Background Sync task to schedule.
     */
    protected boolean scheduleOneOffTask(long minDelayMs, @BackgroundSyncTask int taskType) {
        // Pack SOONEST_EXPECTED_WAKETIME in extras.
        Bundle taskExtras = new Bundle();
        taskExtras.putLong(SOONEST_EXPECTED_WAKETIME, System.currentTimeMillis() + minDelayMs);

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(getAppropriateTaskId(taskType),
                                getAppropriateTaskClass(taskType), minDelayMs, Integer.MAX_VALUE)
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setUpdateCurrent(true)
                        .setIsPersisted(true)
                        .setExtras(taskExtras)
                        .build();
        // This will overwrite any existing task with this ID.
        return BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);
    }

    /**
     * Based on shouldLaunch, either creates or cancels a one-off background
     * task to wake up Chrome upon network connectivity.
     *
     * @param taskType The one-off background task to create.
     * @param shouldLaunch Whether to launch the browser in the background.
     * @param minDelayMs   The minimum time to wait before waking the browser.
     */
    @VisibleForTesting
    @CalledByNative
    protected void launchBrowserIfStopped(
            @BackgroundSyncTask int taskType, boolean shouldLaunch, long minDelayMs) {
        if (!shouldLaunch) {
            cancelOneOffTask(taskType);
            return;
        }

        scheduleOneOffTask(minDelayMs, taskType);
    }

    /**
     * Method for rescheduling a background task to wake up Chrome for processing
     * Background Sync events in the event of an OS upgrade or Google Play Services
     * upgrade.
     *
     * @param taskType The Background Sync task to reschedule.
     */
    public void reschedule(@BackgroundSyncTask int taskType) {
        scheduleOneOffTask(MIN_SYNC_RECOVERY_TIME, taskType);
    }
}

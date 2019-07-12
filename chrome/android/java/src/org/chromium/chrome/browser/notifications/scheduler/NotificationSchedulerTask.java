// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.MainThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * A background task used by notification scheduler system to process and display scheduled
 * notifications.
 */
public class NotificationSchedulerTask extends NativeBackgroundTask {
    private static final String EXTRA_SCHEDULER_TASK_TIME = "extra_scheduler_task_time";
    private static final int NO_EXTRA_SCHEDULER_TASK_TIME = -1;

    @Override
    public void reschedule(Context context) {}

    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // Wrap to a Callback<Boolean> because JNI generator can't recognize TaskFinishedCallback as
        // a Java interface in the function parameter.
        Callback<Boolean> taskCallback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean needsReschedule) {
                callback.taskFinished(needsReschedule);
            }
        };

        @SchedulerTaskTime
        int taskStartTime = taskParameters.getExtras().getInt(
                EXTRA_SCHEDULER_TASK_TIME, NO_EXTRA_SCHEDULER_TASK_TIME);
        nativeOnStartTask(
                Profile.getLastUsedProfile().getOriginalProfile(), taskStartTime, taskCallback);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        // Reschedule the background task if native is not even loaded, that we don't know if any
        // notification needs to be processed.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        @SchedulerTaskTime
        int taskStartTime = taskParameters.getExtras().getInt(
                EXTRA_SCHEDULER_TASK_TIME, NO_EXTRA_SCHEDULER_TASK_TIME);
        return nativeOnStopTask(Profile.getLastUsedProfile().getOriginalProfile(), taskStartTime);
    }

    /**
     * Schedules a notification scheduler background task to display scheduled notifications if
     * needed.
     * @param schedulerTaskTime The time for the task to scheduling e.g. in the morning or evening.
     * @param windowStartMs The starting time of a time window to run the background job in
     *         milliseconds.
     * @param windowEndMs The end time of a time window to run the background job in milliseconds.
     */
    @MainThread
    @CalledByNative
    private static void schedule(
            @SchedulerTaskTime int schedulerTaskTime, long windowStartMs, long windowEndMs) {
        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        Bundle bundle = new Bundle();
        bundle.putInt(EXTRA_SCHEDULER_TASK_TIME, schedulerTaskTime);
        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(TaskIds.NOTIFICATION_SCHEDULER_JOB_ID,
                                NotificationSchedulerTask.class, windowStartMs, windowEndMs)
                        .setUpdateCurrent(true)
                        .setIsPersisted(true)
                        .setExtras(bundle)
                        .build();
        scheduler.schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    /**
     * Cancels the background task for notification scheduler.
     */
    @MainThread
    @CalledByNative
    private static void cancel() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.NOTIFICATION_SCHEDULER_JOB_ID);
    }

    private native void nativeOnStartTask(
            Profile profile, @SchedulerTaskTime int schedulerTaskTime, Callback<Boolean> callback);
    private native boolean nativeOnStopTask(
            Profile profile, @SchedulerTaskTime int schedulerTaskTime);
}

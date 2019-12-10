// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.PowerManager;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Starts running the BackgroundTask at the specified triggering time.
 * TODO (crbug.com/970160): Check that the requirements set for the BackgroundTask are met before
 * starting it.
 *
 * Receives the information through a broadcast, which is synchronous in the Main thread. The
 * execution of the task will be detached to a best effort task.
 */
public class BackgroundTaskBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "BkgrdTaskBR";
    private static final String WAKELOCK_TAG = "Chromium:" + TAG;

    // Wakelock is only held for 3 minutes, in order to be consistent with the restrictions of
    // the GcmTaskService:
    // https://developers.google.com/android/reference/com/google/android/gms/gcm/GcmTaskService.
    // Here the waiting is done for only 90% of this time.
    private static final long MAX_TIMEOUT_MS = 162 * DateUtils.SECOND_IN_MILLIS;

    private static class TaskExecutor implements BackgroundTask.TaskFinishedCallback {
        private final Context mContext;
        private final PowerManager.WakeLock mWakeLock;
        private final TaskParameters mTaskParams;
        private final BackgroundTask mBackgroundTask;

        private boolean mHasExecuted;

        public TaskExecutor(Context context, PowerManager.WakeLock wakeLock,
                TaskParameters taskParams, BackgroundTask backgroundTask) {
            mContext = context;
            mWakeLock = wakeLock;
            mTaskParams = taskParams;
            mBackgroundTask = backgroundTask;
        }

        public void execute() {
            ThreadUtils.assertOnUiThread();

            boolean needsBackground = mBackgroundTask.onStartTask(mContext, mTaskParams, this);
            BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(mTaskParams.getTaskId());
            if (!needsBackground) return;

            PostTask.postDelayedTask(UiThreadTaskTraits.BEST_EFFORT, this::timeout, MAX_TIMEOUT_MS);
        }

        @Override
        public void taskFinished(boolean needsReschedule) {
            PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, () -> finished(needsReschedule));
        }

        private void timeout() {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            Log.w(TAG, "Task execution failed. Task timed out.");
            BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(mTaskParams.getTaskId());

            boolean reschedule = mBackgroundTask.onStopTask(mContext, mTaskParams);
            if (reschedule) mBackgroundTask.reschedule(mContext);
        }

        private void finished(boolean reschedule) {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            if (reschedule) mBackgroundTask.reschedule(mContext);
            // TODO(crbug.com/970160): Add UMA to record how long the tasks need to complete.
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final TaskParameters taskParams =
                BackgroundTaskSchedulerAlarmManager.getTaskParametersFromIntent(intent);
        if (taskParams == null) {
            Log.w(TAG, "Failed to retrieve task parameters.");
            return;
        }

        final BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(taskParams.getTaskId());
        if (backgroundTask == null) {
            Log.w(TAG, "Failed to start task. Could not instantiate BackgroundTask class.");
            // Cancel task if the BackgroundTask class is not found anymore. We assume this means
            // that the task has been deprecated.
            BackgroundTaskSchedulerFactory.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), taskParams.getTaskId());
            return;
        }

        // Keep the CPU on through a wake lock.
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        PowerManager.WakeLock wakeLock =
                powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKELOCK_TAG);
        wakeLock.acquire(MAX_TIMEOUT_MS);

        TaskExecutor taskExecutor = new TaskExecutor(context, wakeLock, taskParams, backgroundTask);
        PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, taskExecutor::execute);
    }
}

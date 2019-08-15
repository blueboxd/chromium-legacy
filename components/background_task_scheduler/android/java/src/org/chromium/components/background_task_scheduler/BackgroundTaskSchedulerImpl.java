// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.os.Build;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.Set;

/**
 * This {@link BackgroundTaskScheduler} is the only one used in production code, and it is used to
 * schedule jobs that run in the background.
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
class BackgroundTaskSchedulerImpl implements BackgroundTaskScheduler {
    private static final String TAG = "BkgrdTaskScheduler";
    private static final String SWITCH_IGNORE_BACKGROUND_TASKS = "ignore-background-tasks";

    private final BackgroundTaskSchedulerDelegate mSchedulerDelegate;

    /** Constructor only for {@link BackgroundTaskSchedulerFactory} and internal component tests. */
    BackgroundTaskSchedulerImpl(BackgroundTaskSchedulerDelegate schedulerDelegate) {
        mSchedulerDelegate = schedulerDelegate;
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        if (CommandLine.getInstance().hasSwitch(SWITCH_IGNORE_BACKGROUND_TASKS)) {
            // When background tasks finish executing, they leave a cached process, which
            // artificially inflates startup metrics that are based on events near to process
            // creation.
            return true;
        }
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskScheduler.schedule", Integer.toString(taskInfo.getTaskId()))) {
            ThreadUtils.assertOnUiThread();
            boolean success = mSchedulerDelegate.schedule(context, taskInfo);
            BackgroundTaskSchedulerUma.getInstance().reportTaskScheduled(
                    taskInfo.getTaskId(), success);
            if (!taskInfo.isPeriodic()) {
                BackgroundTaskSchedulerUma.getInstance().reportTaskCreatedAndExpirationState(
                        taskInfo.getTaskId(), taskInfo.getOneOffInfo().expiresAfterWindowEndTime());
            }
            if (success) {
                BackgroundTaskSchedulerPrefs.addScheduledTask(taskInfo);
            }
            return success;
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        try (TraceEvent te = TraceEvent.scoped(
                     "BackgroundTaskScheduler.cancel", Integer.toString(taskId))) {
            ThreadUtils.assertOnUiThread();
            BackgroundTaskSchedulerUma.getInstance().reportTaskCanceled(taskId);
            BackgroundTaskSchedulerPrefs.removeScheduledTask(taskId);
            mSchedulerDelegate.cancel(context, taskId);
        }
    }

    @Override
    public void checkForOSUpgrade(Context context) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskScheduler.checkForOSUpgrade")) {
            ThreadUtils.assertOnUiThread();
            int oldSdkInt = BackgroundTaskSchedulerPrefs.getLastSdkVersion();
            int newSdkInt = Build.VERSION.SDK_INT;

            // Update tasks stored in the old format to the proto format at Chrome Startup, if
            // tasks are found to be stored in the old format. This allows to keep only one
            // implementation of the storage methods.
            BackgroundTaskSchedulerPrefs.migrateStoredTasksToProto();

            if (oldSdkInt != newSdkInt) {
                // Save the current SDK version to preferences.
                BackgroundTaskSchedulerPrefs.setLastSdkVersion(newSdkInt);
            }

            // No OS upgrade detected or OS upgrade does not change delegate.
            if (oldSdkInt == newSdkInt || !osUpgradeChangesDelegateType(oldSdkInt, newSdkInt)) {
                BackgroundTaskSchedulerUma.getInstance().flushStats();
                return;
            }

            BackgroundTaskSchedulerUma.getInstance().removeCachedStats();

            // Explicitly create and invoke old delegate type to cancel all scheduled tasks.
            // All preference entries are kept until reschedule call, which removes then then.
            BackgroundTaskSchedulerDelegate oldDelegate =
                    BackgroundTaskSchedulerFactory.getSchedulerDelegateForSdk(oldSdkInt);
            Set<Integer> scheduledTaskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
            for (int taskId : scheduledTaskIds) {
                oldDelegate.cancel(context, taskId);
            }

            reschedule(context);
        }
    }

    @Override
    public void reschedule(Context context) {
        try (TraceEvent te = TraceEvent.scoped("BackgroundTaskScheduler.reschedule")) {
            ThreadUtils.assertOnUiThread();
            Set<Integer> scheduledTaskIds = BackgroundTaskSchedulerPrefs.getScheduledTaskIds();
            BackgroundTaskSchedulerPrefs.removeAllTasks();
            for (int taskId : scheduledTaskIds) {
                final BackgroundTask backgroundTask =
                        BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(taskId);
                if (backgroundTask == null) {
                    Log.w(TAG,
                            "Cannot reschedule task for task id " + taskId + ". Could not "
                                    + "instantiate BackgroundTask class.");
                    // Cancel task if the BackgroundTask class is not found anymore. We assume this
                    // means that the task has been deprecated.
                    BackgroundTaskSchedulerFactory.getScheduler().cancel(
                            ContextUtils.getApplicationContext(), taskId);
                    continue;
                }

                backgroundTask.reschedule(context);
            }
        }
    }

    private boolean osUpgradeChangesDelegateType(int oldSdkInt, int newSdkInt) {
        return oldSdkInt < Build.VERSION_CODES.M && newSdkInt >= Build.VERSION_CODES.M;
    }
}

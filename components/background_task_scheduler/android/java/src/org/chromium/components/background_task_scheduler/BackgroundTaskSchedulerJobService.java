// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.List;
/**
 * An implementation of {@link BackgroundTaskSchedulerDelegate} that uses the system
 * {@link JobScheduler} to schedule jobs.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
class BackgroundTaskSchedulerJobService implements BackgroundTaskSchedulerDelegate {
    private static final String TAG = "BkgrdTaskSchedulerJS";

    /** Delta time to use expiration checks. Used to make checks after the end time. */
    static final long DEADLINE_DELTA_MS = 1000;

    /** Clock to use so we can mock time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static Clock sClock = System::currentTimeMillis;

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }

    static BackgroundTask getBackgroundTaskFromJobParameters(JobParameters jobParameters) {
        String backgroundTaskClassName = getBackgroundTaskClassFromJobParameters(jobParameters);
        return BackgroundTaskReflection.getBackgroundTaskFromClassName(backgroundTaskClassName);
    }

    private static String getBackgroundTaskClassFromJobParameters(JobParameters jobParameters) {
        PersistableBundle extras = jobParameters.getExtras();
        if (extras == null) return null;
        return extras.getString(BACKGROUND_TASK_CLASS_KEY);
    }

    static Long getDeadlineTimeFromJobParameters(JobParameters jobParameters) {
        PersistableBundle extras = jobParameters.getExtras();
        if (extras == null || !extras.containsKey(BACKGROUND_TASK_DEADLINE_KEY)) {
            return null;
        }
        return extras.getLong(BACKGROUND_TASK_DEADLINE_KEY);
    }

    private static long getDeadlineTime(TaskInfo taskInfo) {
        long windowEndTimeMs = taskInfo.getOneOffInfo().getWindowEndTimeMs();
        return sClock.currentTimeMillis() + windowEndTimeMs;
    }

    /**
     * Retrieves the {@link TaskParameters} from the {@link JobParameters}, which are passed as
     * one of the keys. Only values valid for {@link android.os.BaseBundle} are supported, and other
     * values are stripped at the time when the task is scheduled.
     *
     * @param jobParameters the {@link JobParameters} to extract the {@link TaskParameters} from.
     * @return the {@link TaskParameters} for the current job.
     */
    static TaskParameters getTaskParametersFromJobParameters(JobParameters jobParameters) {
        TaskParameters.Builder builder = TaskParameters.create(jobParameters.getJobId());

        PersistableBundle jobExtras = jobParameters.getExtras();
        PersistableBundle persistableTaskExtras =
                jobExtras.getPersistableBundle(BACKGROUND_TASK_EXTRAS_KEY);

        Bundle taskExtras = new Bundle();
        taskExtras.putAll(persistableTaskExtras);
        builder.addExtras(taskExtras);

        return builder.build();
    }

    @VisibleForTesting
    static JobInfo createJobInfoFromTaskInfo(Context context, TaskInfo taskInfo) {
        PersistableBundle jobExtras = new PersistableBundle();
        jobExtras.putString(BACKGROUND_TASK_CLASS_KEY, taskInfo.getBackgroundTaskClass().getName());

        if (!taskInfo.isPeriodic() && taskInfo.getOneOffInfo().expiresAfterWindowEndTime()) {
            jobExtras.putLong(BACKGROUND_TASK_DEADLINE_KEY, getDeadlineTime(taskInfo));
        }

        PersistableBundle persistableBundle = getTaskExtrasAsPersistableBundle(taskInfo);
        jobExtras.putPersistableBundle(BACKGROUND_TASK_EXTRAS_KEY, persistableBundle);

        JobInfo.Builder builder =
                new JobInfo
                        .Builder(taskInfo.getTaskId(),
                                new ComponentName(context, BackgroundTaskJobService.class))
                        .setExtras(jobExtras)
                        .setPersisted(taskInfo.isPersisted())
                        .setRequiresCharging(taskInfo.requiresCharging())
                        .setRequiredNetworkType(getJobInfoNetworkTypeFromTaskNetworkType(
                                taskInfo.getRequiredNetworkType()));

        if (taskInfo.isPeriodic()) {
            builder = getPeriodicJobInfo(builder, taskInfo);
        } else {
            builder = getOneOffJobInfo(builder, taskInfo);
        }

        return builder.build();
    }

    private static JobInfo.Builder getOneOffJobInfo(JobInfo.Builder builder, TaskInfo taskInfo) {
        TaskInfo.OneOffInfo oneOffInfo = taskInfo.getOneOffInfo();
        if (oneOffInfo.hasWindowStartTimeConstraint()) {
            builder = builder.setMinimumLatency(oneOffInfo.getWindowStartTimeMs());
        }
        long windowEndTimeMs = oneOffInfo.getWindowEndTimeMs();
        if (oneOffInfo.expiresAfterWindowEndTime()) {
            windowEndTimeMs += DEADLINE_DELTA_MS;
        }
        return builder.setOverrideDeadline(windowEndTimeMs);
    }

    private static JobInfo.Builder getPeriodicJobInfo(JobInfo.Builder builder, TaskInfo taskInfo) {
        TaskInfo.PeriodicInfo periodicInfo = taskInfo.getPeriodicInfo();
        if (periodicInfo.hasFlex()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                return builder.setPeriodic(periodicInfo.getIntervalMs(), periodicInfo.getFlexMs());
            }
            return builder.setPeriodic(periodicInfo.getIntervalMs());
        }
        return builder.setPeriodic(periodicInfo.getIntervalMs());
    }

    private static int getJobInfoNetworkTypeFromTaskNetworkType(
            @TaskInfo.NetworkType int networkType) {
        // The values are hard coded to represent the same as the network type from JobService.
        return networkType;
    }

    private static PersistableBundle getTaskExtrasAsPersistableBundle(TaskInfo taskInfo) {
        Bundle taskExtras = taskInfo.getExtras();
        BundleToPersistableBundleConverter.Result convertedData =
                BundleToPersistableBundleConverter.convert(taskExtras);
        if (convertedData.hasErrors()) {
            Log.w(TAG, "Failed converting extras to PersistableBundle: "
                            + convertedData.getFailedKeysErrorString());
        }
        return convertedData.getPersistableBundle();
    }

    @Override
    public boolean schedule(Context context, TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();
        if (!BackgroundTaskReflection.hasParameterlessPublicConstructor(
                    taskInfo.getBackgroundTaskClass())) {
            Log.e(TAG, "BackgroundTask " + taskInfo.getBackgroundTaskClass()
                            + " has no parameterless public constructor.");
            return false;
        }

        JobInfo jobInfo = createJobInfoFromTaskInfo(context, taskInfo);

        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);

        if (!taskInfo.shouldUpdateCurrent() && hasPendingJob(jobScheduler, taskInfo.getTaskId())) {
            return true;
        }
        // This can fail on heavily modified android builds.  Catch so we don't crash.
        try {
            return jobScheduler.schedule(jobInfo) == JobScheduler.RESULT_SUCCESS;
        } catch (Exception e) {
            // Typically we don't catch RuntimeException, but this time we do want to catch it
            // because we are worried about android as modified by device manufacturers.
            Log.e(TAG, "Unable to schedule with Android.", e);
            return false;
        }
    }

    @Override
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();
        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        try {
            jobScheduler.cancel(taskId);
        } catch (NullPointerException exception) {
            Log.e(TAG, "Failed to cancel task: " + taskId);
        }
    }

    private boolean hasPendingJob(JobScheduler jobScheduler, int jobId) {
        List<JobInfo> pendingJobs = jobScheduler.getAllPendingJobs();
        for (JobInfo pendingJob : pendingJobs) {
            if (pendingJob.getId() == jobId) return true;
        }

        return false;
    }
}

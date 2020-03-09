// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedFetchInfo;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.util.concurrent.TimeUnit;

/**
 * AwVariationsSeedFetcher is a JobService which periodically downloads the variations seed. We use
 * JobService instead of BackgroundTaskScheduler, since JobService is available on L+, and WebView
 * is L+ only. The job is scheduled whenever an app requests the seed, and it's been at least 1 day
 * since the last fetch. If WebView is never used, the job will never run. The 1-day minimum fetch
 * period is chosen as a trade-off between seed freshness (and prompt delivery of feature
 * killswitches) and data and battery usage. Various Android versions may enforce longer periods,
 * depending on WebView usage and battery-saving features. AwVariationsSeedFetcher is not meant to
 * be used outside the variations service. For the equivalent fetch in Chrome, see
 * AsyncInitTaskRunner$FetchSeedTask.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP) // for JobService
public class AwVariationsSeedFetcher extends JobService {
    private static final String TAG = "AwVariationsSeedFet-";
    private static final int JOB_ID = TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID;
    private static final long MIN_JOB_PERIOD_MILLIS = TimeUnit.HOURS.toMillis(12);

    /** Clock used to fake time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static JobScheduler sMockJobScheduler;
    private static VariationsSeedFetcher sMockDownloader;
    private static Clock sTestClock;

    private FetchTask mFetchTask;

    private static long currentTimeMillis() {
        if (sTestClock != null) {
            return sTestClock.currentTimeMillis();
        }
        return System.currentTimeMillis();
    }

    private static String getChannelStr() {
        switch (VersionConstants.CHANNEL) {
            case Channel.STABLE: return "stable";
            case Channel.BETA:   return "beta";
            case Channel.DEV:    return "dev";
            case Channel.CANARY: return "canary";
            default: return null;
        }
    }

    // Use JobScheduler.getPendingJob() if it's available. Otherwise, fall back to iterating over
    // all jobs to find the one we want.
    private static JobInfo getPendingJob(JobScheduler scheduler, int jobId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            for (JobInfo info : scheduler.getAllPendingJobs()) {
                if (info.getId() == jobId) return info;
            }
            return null;
        }
        return ApiHelperForN.getPendingJob(scheduler, jobId);
    }

    private static JobScheduler getScheduler() {
        if (sMockJobScheduler != null) return sMockJobScheduler;

        // This may be null due to vendor framework bugs. https://crbug.com/968636
        return (JobScheduler) ContextUtils.getApplicationContext().getSystemService(
                Context.JOB_SCHEDULER_SERVICE);
    }

    public static void scheduleIfNeeded() {
        JobScheduler scheduler = getScheduler();
        if (scheduler == null) return;

        // Check if it's already scheduled.
        if (!CommandLine.getInstance().hasSwitch(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD)
                && getPendingJob(scheduler, JOB_ID) != null) {
            VariationsUtils.debugLog("Seed download job already scheduled");
            return;
        }

        // Check how long it's been since FetchTask last ran.
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime != 0) {
            long now = currentTimeMillis();
            long minJobPeriodMillis = VariationsUtils.getDurationSwitchValueInMillis(
                    AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD, MIN_JOB_PERIOD_MILLIS);
            if (now < lastRequestTime + minJobPeriodMillis) {
                VariationsUtils.debugLog("Throttling seed download job");
                return;
            }
        }

        VariationsUtils.debugLog("Scheduling seed download job");
        Context context = ContextUtils.getApplicationContext();
        ComponentName thisComponent = new ComponentName(context, AwVariationsSeedFetcher.class);
        JobInfo job = new JobInfo.Builder(JOB_ID, thisComponent)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setRequiresCharging(true)
                .build();
        if (scheduler.schedule(job) == JobScheduler.RESULT_SUCCESS) {
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            metrics.setLastEnqueueTime(currentTimeMillis());
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        } else {
            Log.e(TAG, "Failed to schedule job");
        }
    }

    private class FetchTask extends BackgroundOnlyAsyncTask<Void> {
        private JobParameters mParams;

        FetchTask(JobParameters params) {
            mParams = params;
        }

        @Override
        protected Void doInBackground() {
            // Should we call jobFinished at the end of this task?
            boolean shouldFinish = true;
            long startTime = currentTimeMillis();

            try {
                VariationsUtils.updateStampTime();

                VariationsUtils.debugLog("Downloading new seed");
                VariationsSeedFetcher downloader =
                        sMockDownloader != null ? sMockDownloader : VariationsSeedFetcher.get();
                String milestone = String.valueOf(VersionConstants.PRODUCT_MAJOR_VERSION);
                SeedFetchInfo fetchInfo = downloader.downloadContent(
                        VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW,
                        /*restrictMode=*/null, milestone, getChannelStr());

                saveMetrics(fetchInfo.seedFetchResult, startTime, /*endTime=*/currentTimeMillis());

                if (isCancelled()) {
                    return null;
                }

                if (fetchInfo.seedInfo != null) {
                    VariationsSeedHolder.getInstance().updateSeed(
                            fetchInfo.seedInfo, /*onFinished=*/() -> jobFinished(mParams));
                    shouldFinish = false; // jobFinished will be deferred until updateSeed is done.
                }
            } finally {
                if (shouldFinish) jobFinished(mParams);
            }

            return null;
        }

        private void saveMetrics(int seedFetchResult, long startTime, long endTime) {
            Context context = ContextUtils.getApplicationContext();
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            metrics.setSeedFetchResult(seedFetchResult);
            metrics.setSeedFetchTime(endTime - startTime);
            if (metrics.hasLastEnqueueTime()) {
                metrics.setJobQueueTime(startTime - metrics.getLastEnqueueTime());
            }
            if (metrics.hasLastJobStartTime()) {
                metrics.setJobInterval(startTime - metrics.getLastJobStartTime());
            }
            metrics.clearLastEnqueueTime();
            metrics.setLastJobStartTime(startTime);
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        }
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        // If this process has survived since the last run of this job, mFetchTask could still
        // exist. Either way, (re)create it with the new params.
        mFetchTask = new FetchTask(params);
        mFetchTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        if (mFetchTask != null) {
            mFetchTask.cancel(true);
            mFetchTask = null;
        }
        return false;
    }

    protected void jobFinished(JobParameters params) {
        assert params.getJobId() == JOB_ID;
        jobFinished(params, /*needsReschedule=*/false);
    }

    public static void setMocks(JobScheduler scheduler, VariationsSeedFetcher fetcher) {
        sMockJobScheduler = scheduler;
        sMockDownloader = fetcher;
    }

    public static void setTestClock(Clock clock) {
        sTestClock = clock;
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ContentResolver;
import android.os.Environment;
import android.os.StatFs;
import android.provider.Settings;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for WebAPKs. NOTE: Histogram names and values are defined in
 * tools/metrics/histograms/histograms.xml. Please update that file if any change is made.
 * TODO(crbug.com/1219648): Temporarily keep this around to avoid breaking down stream. This
 * class should be removed once down stream is updated.
 */
public class WebApkUma {
    private static final long WEBAPK_EXTRA_INSTALLATION_SPACE_BYTES =
            100 * (long) ConversionUtils.BYTES_PER_MEGABYTE; // 100 MB

    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    @IntDef({GooglePlayInstallResult.SUCCESS, GooglePlayInstallResult.FAILED_NO_DELEGATE,
            GooglePlayInstallResult.FAILED_TO_CONNECT_TO_SERVICE,
            GooglePlayInstallResult.FAILED_CALLER_VERIFICATION_FAILURE,
            GooglePlayInstallResult.FAILED_POLICY_VIOLATION,
            GooglePlayInstallResult.FAILED_API_DISABLED,
            GooglePlayInstallResult.FAILED_REQUEST_FAILED,
            GooglePlayInstallResult.FAILED_DOWNLOAD_CANCELLED,
            GooglePlayInstallResult.FAILED_DOWNLOAD_ERROR,
            GooglePlayInstallResult.FAILED_INSTALL_ERROR,
            GooglePlayInstallResult.FAILED_INSTALL_TIMEOUT,
            GooglePlayInstallResult.REQUEST_FAILED_POLICY_DISABLED,
            GooglePlayInstallResult.REQUEST_FAILED_UNKNOWN_ACCOUNT,
            GooglePlayInstallResult.REQUEST_FAILED_NETWORK_ERROR,
            GooglePlayInstallResult.REQUSET_FAILED_RESOLVE_ERROR,
            GooglePlayInstallResult.REQUEST_FAILED_NOT_GOOGLE_SIGNED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface GooglePlayInstallResult {
        int SUCCESS = 0;
        int FAILED_NO_DELEGATE = 1;
        int FAILED_TO_CONNECT_TO_SERVICE = 2;
        int FAILED_CALLER_VERIFICATION_FAILURE = 3;
        int FAILED_POLICY_VIOLATION = 4;
        int FAILED_API_DISABLED = 5;
        int FAILED_REQUEST_FAILED = 6;
        int FAILED_DOWNLOAD_CANCELLED = 7;
        int FAILED_DOWNLOAD_ERROR = 8;
        int FAILED_INSTALL_ERROR = 9;
        int FAILED_INSTALL_TIMEOUT = 10;
        // REQUEST_FAILED_* errors are the error codes shown in the "reason" of
        // the returned Bundle when calling installPackage() API returns false.
        int REQUEST_FAILED_POLICY_DISABLED = 11;
        int REQUEST_FAILED_UNKNOWN_ACCOUNT = 12;
        int REQUEST_FAILED_NETWORK_ERROR = 13;
        int REQUSET_FAILED_RESOLVE_ERROR = 14;
        int REQUEST_FAILED_NOT_GOOGLE_SIGNED = 15;
        int NUM_ENTRIES = 16;
    }

    /**
     * Records whether installing a WebAPK from Google Play succeeded. If not, records the reason
     * that the install failed.
     */
    public static void recordGooglePlayInstallResult(@GooglePlayInstallResult int result) {
        RecordHistogram.recordEnumeratedHistogram("WebApk.Install.GooglePlayInstallResult", result,
                GooglePlayInstallResult.NUM_ENTRIES);
    }

    /** Records the error code if installing a WebAPK via Google Play fails. */
    public static void recordGooglePlayInstallErrorCode(int errorCode) {
        // Don't use an enumerated histogram as there are > 30 potential error codes. In practice,
        // a given client will always get the same error code.
        RecordHistogram.recordSparseHistogram(
                "WebApk.Install.GooglePlayErrorCode", Math.min(errorCode, 1000));
    }

    /**
     * Records whether updating a WebAPK from Google Play succeeded. If not, records the reason
     * that the update failed.
     */
    public static void recordGooglePlayUpdateResult(@GooglePlayInstallResult int result) {
        RecordHistogram.recordEnumeratedHistogram("WebApk.Update.GooglePlayUpdateResult", result,
                GooglePlayInstallResult.NUM_ENTRIES);
    }

    /**
     * Log necessary disk usage and cache size UMAs when WebAPK installation fails.
     */
    public static void logSpaceUsageUMAWhenInstallationFails() {
        new AsyncTask<Void>() {
            long mAvailableSpaceInByte;
            long mCacheSizeInByte;
            @Override
            protected Void doInBackground() {
                mAvailableSpaceInByte = getAvailableSpaceAboveLowSpaceLimit();
                mCacheSizeInByte = getCacheDirSize();
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                logSpaceUsageUMAOnDataAvailable(mAvailableSpaceInByte, mCacheSizeInByte);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private static void logSpaceUsageUMAOnDataAvailable(long spaceSize, long cacheSize) {
        RecordHistogram.recordSparseHistogram(
                "WebApk.Install.AvailableSpace.Fail", roundByteToMb(spaceSize));

        RecordHistogram.recordSparseHistogram("WebApk.Install.AvailableSpaceAfterFreeUpCache.Fail",
                roundByteToMb(spaceSize + cacheSize));
    }

    private static int roundByteToMb(long bytes) {
        int mbs = (int) (bytes / (long) ConversionUtils.BYTES_PER_MEGABYTE / 10L * 10L);
        return Math.min(1000, Math.max(-1000, mbs));
    }

    private static long getDirectorySizeInByte(File dir) {
        if (dir == null) return 0;
        if (!dir.isDirectory()) return dir.length();

        long sizeInByte = 0;
        try {
            File[] files = dir.listFiles();
            if (files == null) return 0;
            for (File file : files) sizeInByte += getDirectorySizeInByte(file);
        } catch (SecurityException e) {
            return 0;
        }
        return sizeInByte;
    }

    /**
     * @return The available space that can be used to install WebAPK. Negative value means there is
     * less free space available than the system's minimum by the given amount.
     */
    public static long getAvailableSpaceAboveLowSpaceLimit() {
        StatFs partitionStats = new StatFs(Environment.getDataDirectory().getAbsolutePath());
        long partitionAvailableBytes = partitionStats.getAvailableBytes();
        long partitionTotalBytes = partitionStats.getTotalBytes();
        long minimumFreeBytes = getLowSpaceLimitBytes(partitionTotalBytes);

        long webApkExtraSpaceBytes = WEBAPK_EXTRA_INSTALLATION_SPACE_BYTES;
        return partitionAvailableBytes - minimumFreeBytes + webApkExtraSpaceBytes;
    }

    /**
     * @return Size of the cache directory.
     */
    public static long getCacheDirSize() {
        return getDirectorySizeInByte(ContextUtils.getApplicationContext().getCacheDir());
    }

    /**
     * Mirror the system-derived calculation of reserved bytes and return that value.
     */
    private static long getLowSpaceLimitBytes(long partitionTotalBytes) {
        // Copied from android/os/storage/StorageManager.java
        final int defaultThresholdPercentage = 10;
        // Copied from android/os/storage/StorageManager.java
        final long defaultThresholdMaxBytes = 500 * ConversionUtils.BYTES_PER_MEGABYTE;
        // Copied from android/provider/Settings.java
        final String sysStorageThresholdPercentage = "sys_storage_threshold_percentage";
        // Copied from android/provider/Settings.java
        final String sysStorageThresholdMaxBytes = "sys_storage_threshold_max_bytes";

        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        int minFreePercent = 0;
        long minFreeBytes = 0;

        // Retrieve platform-appropriate values first
        minFreePercent = Settings.Global.getInt(
                resolver, sysStorageThresholdPercentage, defaultThresholdPercentage);
        minFreeBytes = Settings.Global.getLong(
                resolver, sysStorageThresholdMaxBytes, defaultThresholdMaxBytes);

        long minFreePercentInBytes = (partitionTotalBytes * minFreePercent) / 100;

        return Math.min(minFreeBytes, minFreePercentInBytes);
    }
}

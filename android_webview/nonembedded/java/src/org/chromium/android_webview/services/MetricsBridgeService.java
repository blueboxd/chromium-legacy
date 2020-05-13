// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecordList;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/**
 * Service that keeps record of UMA method calls in nonembedded WebView processes.
 */
public final class MetricsBridgeService extends Service {
    private static final String TAG = "MetricsBridgeService";

    // Max histograms this service will store, arbitrarily chosen
    private static final int MAX_HISTOGRAM_COUNT = 512;

    private static final String LOG_FILE_NAME = "webview_metrics_bridge_logs";

    private final File mLogFile;

    // Not guarded by a lock because it should only be accessed in a SequencedTaskRunner.
    private HistogramRecordList mRecordsList = HistogramRecordList.newBuilder().build();

    // To avoid any potential synchronization issues as well as avoid blocking the caller thread
    // (e.g when the caller is a thread from the same process.), we post all read/write operations
    // to be run serially using a SequencedTaskRunner instead of using a lock.
    private final TaskRunner mSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    @Override
    public void onCreate() {
        // Restore saved histograms from disk.
        mSequencedTaskRunner.postTask(() -> {
            File file = getMetricsLogFile();
            if (!file.exists()) return;
            try (FileInputStream in = new FileInputStream(file)) {
                HistogramRecordList.Builder listBuilder = HistogramRecordList.newBuilder();
                HistogramRecord savedProto = HistogramRecord.parseDelimitedFrom(in);
                while (savedProto != null) {
                    listBuilder.addRecords(savedProto);
                    savedProto = HistogramRecord.parseDelimitedFrom(in);
                }
                mRecordsList = listBuilder.build();
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "Malformed metrics log proto", e);
                deleteMetricsLogFile();
            } catch (IOException e) {
                Log.e(TAG, "Failed reading proto log file", e);
            }
        });
    }

    public MetricsBridgeService() {
        this(new File(PathUtils.getDataDirectory(), LOG_FILE_NAME));
    }

    @VisibleForTesting
    // Inject a logFile for testing.
    public MetricsBridgeService(File logFile) {
        mLogFile = logFile;
    }

    private final IMetricsBridgeService.Stub mBinder = new IMetricsBridgeService.Stub() {
        @Override
        public void recordMetrics(byte[] data) {
            if (Binder.getCallingUid() != Process.myUid()) {
                throw new SecurityException(
                        "recordMetrics() may only be called by non-embedded WebView processes");
            }
            // If this is called within the same process, it will run as a sync call blocking
            // the caller thread, so we will always punt this to thread pool.
            mSequencedTaskRunner.postTask(() -> {
                // Make sure that we don't add records indefinitely in case of no embedded
                // WebView connects to the service to retrieve and clear the records.
                if (mRecordsList.getRecordsCount() >= MAX_HISTOGRAM_COUNT) {
                    // TODO(https://crbug.com/1073683) add a histogram to log request overflow.
                    Log.w(TAG, "retained records has reached the max capacity, dropping record");
                    return;
                }

                HistogramRecord proto = null;
                try {
                    proto = HistogramRecord.parseFrom(data);
                    mRecordsList = mRecordsList.toBuilder().addRecords(proto).build();
                } catch (InvalidProtocolBufferException e) {
                    Log.e(TAG, "Malformed metrics log proto", e);
                    return;
                }

                // Append the histogram record to log file.
                try (FileOutputStream out =
                                new FileOutputStream(getMetricsLogFile(), /* append */ true)) {
                    proto.writeDelimitedTo(out);
                } catch (IOException e) {
                    Log.e(TAG, "Failed to write to file", e);
                }
            });
        }

        @Override
        public byte[] retrieveNonembeddedMetrics() {
            FutureTask<byte[]> retrieveFutureTask = new FutureTask<>(() -> {
                byte[] data = mRecordsList.toByteArray();
                mRecordsList = HistogramRecordList.newBuilder().build();
                deleteMetricsLogFile();
                return data;
            });
            mSequencedTaskRunner.postTask(retrieveFutureTask);
            try {
                return retrieveFutureTask.get();
            } catch (ExecutionException | InterruptedException e) {
                Log.e(TAG, "error executing retrieveNonembeddedMetrics future task", e);
                return new byte[0];
            }
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private File getMetricsLogFile() {
        return mLogFile;
    }

    private boolean deleteMetricsLogFile() {
        return getMetricsLogFile().delete();
    }

    /**
     * Block until all the tasks in the local {@code mSequencedTaskRunner} are finished.
     *
     * @param timeoutMillis timeout in milliseconds.
     */
    @VisibleForTesting
    public FutureTask addTaskToBlock() {
        FutureTask<Object> blockTask = new FutureTask<Object>(() -> {}, new Object());
        mSequencedTaskRunner.postTask(blockTask);
        return blockTask;
    }
}
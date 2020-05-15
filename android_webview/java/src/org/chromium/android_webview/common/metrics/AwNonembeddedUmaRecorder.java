// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.common.metrics;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.UmaRecorder;

/**
 * {@link UmaRecorder} for nonembedded WebView processes.
 * Can be used as a delegate in {@link org.chromium.base.metrics.UmaRecorderHolder}. This may only
 * be called from non-embedded WebView processes, such as developer UI or Services.
 */
public class AwNonembeddedUmaRecorder implements UmaRecorder {
    private static final String TAG = "AwNonembedUmaRecord";

    private final String mServiceName;

    @VisibleForTesting
    public AwNonembeddedUmaRecorder(String serviceName) {
        mServiceName = serviceName;
    }

    public AwNonembeddedUmaRecorder() {
        this(ServiceNames.METRICS_BRIDGE_SERVICE);
    }

    /** Records a single sample of a boolean histogram. */
    @Override
    public void recordBooleanHistogram(String name, boolean sample) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_BOOLEAN);
        builder.setHistogramName(name);
        builder.setSample(sample ? 1 : 0);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a histogram with exponentially scaled buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#count-histograms}
     * <p>
     * This is the default histogram type used by "counts", "times" and "memory" histograms in
     * {@link org.chromium.base.metrics.RecordHistogram}
     *
     * @param min the smallest value recorded in the first bucket; should be greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    @Override
    public void recordExponentialHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_EXPONENTIAL);
        builder.setHistogramName(name);
        builder.setSample(sample);
        builder.setMin(min);
        builder.setMax(max);
        builder.setNumBuckets(numBuckets);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a histogram with evenly spaced buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#percentage-or-ratio-histograms}
     * <p>
     * This histogram type is best suited for recording enums, percentages and ratios.
     *
     * @param min the smallest value recorded in the first bucket; should be equal to one, but will
     *         work with values greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets evenly cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_LINEAR);
        builder.setHistogramName(name);
        builder.setSample(sample);
        builder.setMin(min);
        builder.setMax(max);
        builder.setNumBuckets(numBuckets);

        recordHistogram(builder.build());
    }

    /**
     * Records a single sample of a sparse histogram. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#when-to-use-sparse-histograms}
     */
    @Override
    public void recordSparseHistogram(String name, int sample) {
        HistogramRecord.Builder builder = HistogramRecord.newBuilder();
        builder.setRecordType(RecordType.HISTOGRAM_SPARSE);
        builder.setHistogramName(name);
        builder.setSample(sample);

        recordHistogram(builder.build());
    }

    /**
     * Records a user action. Action names must be documented in {@code actions.xml}. See {@link
     * https://source.chromium.org/chromium/chromium/src/+/master:tools/metrics/actions/README.md}
     *
     * @param name Name of the user action.
     * @param elapsedRealtimeMillis Value of {@link android.os.SystemClock.elapsedRealtime()} when
     *         the action was observed.
     */
    @Override
    public void recordUserAction(String name, long elapsedRealtimeMillis) {
        // Assertion is disabled in release
        // TODO(https://crbug.com/1082475) support recording user actions.
        assert false : "Recording UserAction in non-embedded WebView processes isn't supported yet";
    }

    // Connects to the MetricsBridgeService to record the histogram call.
    private void recordHistogram(HistogramRecord methodCall) {
        final Context appContext = ContextUtils.getApplicationContext();
        final Intent intent = new Intent();
        intent.setClassName(appContext, mServiceName);

        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName className, IBinder service) {
                IMetricsBridgeService metricsService =
                        IMetricsBridgeService.Stub.asInterface(service);
                try {
                    // We are not punting this to a background thread since the cost of IPC itself
                    // should be relatively cheap, and the remote method does its work
                    // asynchronously.
                    metricsService.recordMetrics(methodCall.toByteArray());
                } catch (RemoteException e) {
                    Log.e(TAG, "Remote Exception calling IMetricsBridgeService#recordMetrics", e);
                } finally {
                    appContext.unbindService(this);
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName className) {}
        };
        // TODO(https://crbug.com/1081262) bind to the service once
        if (!appContext.bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
            Log.w(TAG, "Could not bind to MetricsBridgeService " + intent);
        }
    }
}
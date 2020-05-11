// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.common.metrics;

import android.os.Bundle;

import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Replay the recorded method calls recorded by {@link AwProcessUmaRecorder}.
 *
 * Should be used in processes which have initialized Uma, such as the browser process.
 */
public class AwNonembeddedUmaReplayer {
    private static final String TAG = "AwNonembedUmaReplay";

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.RecordHistogram#recordBooleanHistogram}.
     */
    private static void replayBooleanHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_BOOLEAN;

        int sample = proto.getSample();
        if (sample != 0 && sample != 1) {
            Log.d(TAG, "Expected BooleanHistogram to have sample of 0 or 1, but was " + sample);
            return;
        }

        RecordHistogram.recordBooleanHistogram(proto.getHistogramName(), proto.getSample() != 0);
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.RecordHistogram#recordCustomCountHistogram}.
     */
    private static void replayExponentialHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_EXPONENTIAL;

        RecordHistogram.recordCustomCountHistogram(proto.getHistogramName(), proto.getSample(),
                proto.getMin(), proto.getMax(), proto.getNumBuckets());
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.RecordHistogram#recordLinearCountHistogram}.
     */
    private static void replayLinearHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_LINEAR;

        RecordHistogram.recordLinearCountHistogram(proto.getHistogramName(), proto.getSample(),
                proto.getMin(), proto.getMax(), proto.getNumBuckets());
    }

    /**
     * Extract method arguments from the given {@link HistogramRecord} and call
     * {@link org.chromium.base.metrics.RecordHistogram#recordSparseHistogram}.
     */
    private static void replaySparseHistogram(HistogramRecord proto) {
        assert proto.getRecordType() == RecordType.HISTOGRAM_SPARSE;

        RecordHistogram.recordSparseHistogram(proto.getHistogramName(), proto.getSample());
    }

    /**
     * Replay UMA API call record by calling that API method.
     *
     * @param methodCall {@link Bundle} that contains the UMA API type and arguments.
     */
    public static void replayMethodCall(HistogramRecord methodCall) {
        switch (methodCall.getRecordType()) {
            case HISTOGRAM_BOOLEAN:
                replayBooleanHistogram(methodCall);
                break;
            case HISTOGRAM_EXPONENTIAL:
                replayExponentialHistogram(methodCall);
                break;
            case HISTOGRAM_LINEAR:
                replayLinearHistogram(methodCall);
                break;
            case HISTOGRAM_SPARSE:
                replaySparseHistogram(methodCall);
                break;
            default:
                Log.d(TAG, "Unrecognized record type");
        }
    }

    // Don't instantiate this class.
    private AwNonembeddedUmaReplayer() {}
}
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.common.metrics;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.common.metrics.AwNonembeddedUmaReplayer;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Test AwNonembeddedUmaReplayer.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class AwNonembeddedUmaReplayerTest {
    @Test
    @SmallTest
    public void testReplayBooleanHistogram() {
        String histogramName = "testReplayTrueBooleanHistogram";
        HistogramRecord trueHistogramProto = HistogramRecord.newBuilder()
                                                     .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                                     .setHistogramName(histogramName)
                                                     .setSample(1)
                                                     .build();
        HistogramRecord falseHistogramProto = trueHistogramProto.toBuilder().setSample(0).build();
        HistogramRecord inValidHistogramProto =
                trueHistogramProto.toBuilder().setSample(55).build();
        AwNonembeddedUmaReplayer.replayMethodCall(trueHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(trueHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(falseHistogramProto);
        AwNonembeddedUmaReplayer.replayMethodCall(inValidHistogramProto);
        Assert.assertEquals(3, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        Assert.assertEquals(2, RecordHistogram.getHistogramValueCountForTesting(histogramName, 1));
        Assert.assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(histogramName, 0));
    }

    @Test
    @SmallTest
    public void testReplayExponentialHistogram() {
        String histogramName = "testReplayExponentialHistogram";
        int sample = 100;
        int min = 5;
        int max = 1000;
        int numBuckets = 20;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_EXPONENTIAL)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .setMin(min)
                                              .setMax(max)
                                              .setNumBuckets(numBuckets)
                                              .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(histogramName, sample));
    }

    @Test
    @SmallTest
    public void testReplayLinearHistogram() {
        String histogramName = "testReplayLinearHistogram";
        int sample = 100;
        int min = 5;
        int max = 1000;
        int numBuckets = 20;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_LINEAR)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .setMin(min)
                                              .setMax(max)
                                              .setNumBuckets(numBuckets)
                                              .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(histogramName, sample));
    }

    @Test
    @SmallTest
    public void testReplaySparseHistogram() {
        String histogramName = "testReplaySparseHistogram";
        int sample = 10;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .build();
        AwNonembeddedUmaReplayer.replayMethodCall(recordProto);
        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(histogramName, sample));
    }
}
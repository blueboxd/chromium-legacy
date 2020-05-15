// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.common.metrics;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.metrics.AwNonembeddedUmaRecorder;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.android_webview.test.services.MockMetricsBridgeService;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;

/**
 * Test AwNonembeddedUmaRecorder.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class AwNonembeddedUmaRecorderTest {
    private static final long BINDER_TIMEOUT_MILLIS = 10000;

    private AwNonembeddedUmaRecorder mUmaRecorder;

    @Before
    public void setUp() {
        mUmaRecorder = new AwNonembeddedUmaRecorder(MockMetricsBridgeService.class.getName());
    }

    @Test
    @MediumTest
    public void testRecordTrueBooleanHistogram() {
        String histogramName = "testRecordBooleanHistogram";
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                              .setHistogramName(histogramName)
                                              .setSample(1)
                                              .build();
        mUmaRecorder.recordBooleanHistogram(histogramName, true);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordFalseBooleanHistogram() {
        String histogramName = "testRecordBooleanHistogram";
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                              .setHistogramName(histogramName)
                                              .setSample(0)
                                              .build();
        mUmaRecorder.recordBooleanHistogram(histogramName, false);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordExponentialHistogram() {
        String histogramName = "recordExponentialHistogram";
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
        mUmaRecorder.recordExponentialHistogram(histogramName, sample, min, max, numBuckets);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordLinearHistogram() {
        String histogramName = "testRecordLinearHistogram";
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
        mUmaRecorder.recordLinearHistogram(histogramName, sample, min, max, numBuckets);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordSparseHistogram() {
        String histogramName = "testRecordSparseHistogram";
        int sample = 10;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .build();
        mUmaRecorder.recordSparseHistogram(histogramName, sample);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    // Test calling RecordHistogram class methods to make sure a record is delegated as expected.
    public void testRecordHistogram() {
        String histogramName = "testRecordHistogram.testRecordSparseHistogram";
        int sample = 10;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .build();
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);
        RecordHistogram.recordSparseHistogram(histogramName, sample);
        byte[] recordedData = MockMetricsBridgeService.getRecordedData();
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }
}
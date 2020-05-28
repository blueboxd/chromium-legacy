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
    private static final String TEST_SERVICE_NAME = MockMetricsBridgeService.class.getName();

    private AwNonembeddedUmaRecorder mUmaRecorder;

    @Before
    public void setUp() {
        mUmaRecorder = new AwNonembeddedUmaRecorder(TEST_SERVICE_NAME);
        // Reset static variables in case when the service is still running from a previous test.
        MockMetricsBridgeService.reset();
    }

    @Test
    @MediumTest
    public void testRecordTrueBooleanHistogram() throws Throwable {
        String histogramName = "testRecordBooleanHistogram";
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                              .setHistogramName(histogramName)
                                              .setSample(1)
                                              .build();
        mUmaRecorder.recordBooleanHistogram(histogramName, true);
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordFalseBooleanHistogram() throws Throwable {
        String histogramName = "testRecordBooleanHistogram";
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                              .setHistogramName(histogramName)
                                              .setSample(0)
                                              .build();
        mUmaRecorder.recordBooleanHistogram(histogramName, false);
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordExponentialHistogram() throws Throwable {
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
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordLinearHistogram() throws Throwable {
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
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordSparseHistogram() throws Throwable {
        String histogramName = "testRecordSparseHistogram";
        int sample = 10;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .build();
        mUmaRecorder.recordSparseHistogram(histogramName, sample);
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    public void testRecordMultipleHistograms() throws Throwable {
        String histogramName = "testRecordMultipleSparseHistograms";
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(3)
                                              .build();

        mUmaRecorder.recordSparseHistogram(histogramName, 1);
        mUmaRecorder.recordSparseHistogram(histogramName, 2);
        mUmaRecorder.recordSparseHistogram(histogramName, 3);

        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(3);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }

    @Test
    @MediumTest
    @SuppressWarnings("ThreadPriorityCheck")
    public void testStressParallelHistograms() throws Exception {
        final int numThreads = 16;
        final int numSamples = AwNonembeddedUmaRecorder.MAX_PENDING_RECORDS_COUNT / numThreads;

        Thread[] threads = new Thread[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = new Thread(() -> {
                for (int j = 0; j < numSamples; j++) {
                    mUmaRecorder.recordSparseHistogram("StressTest", 10);
                    // Make it more likely this thread will be preempted.
                    Thread.yield();
                }
            });
            threads[i].start();
        }
        for (Thread thread : threads) {
            thread.join();
        }
        MockMetricsBridgeService.getReceivedDataAfter(numSamples * numThreads);
    }

    @Test
    @MediumTest
    // Test calling RecordHistogram class methods to make sure a record is delegated as expected.
    public void testRecordHistogram() throws Throwable {
        String histogramName = "testRecordHistogram.testRecordSparseHistogram";
        int sample = 10;
        HistogramRecord recordProto = HistogramRecord.newBuilder()
                                              .setRecordType(RecordType.HISTOGRAM_SPARSE)
                                              .setHistogramName(histogramName)
                                              .setSample(sample)
                                              .build();
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorder);
        RecordHistogram.recordSparseHistogram(histogramName, sample);
        byte[] recordedData = MockMetricsBridgeService.getReceivedDataAfter(1);
        Assert.assertArrayEquals(recordProto.toByteArray(), recordedData);
    }
}
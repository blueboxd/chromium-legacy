// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.ConditionVariable;
import android.os.IBinder;
import android.os.RemoteException;
import android.support.test.filters.MediumTest;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecordList;
import org.chromium.android_webview.services.MetricsBridgeService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.FutureTask;

/**
 * Test MetricsBridgeService.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class MetricsBridgeServiceTest {
    private static final long BINDER_TIMEOUT_MILLIS = 10000;

    private File mTempFile;

    @Before
    public void setUp() throws IOException {
        mTempFile = File.createTempFile("test_webview_metrics_bridge_logs", null);
    }

    @After
    public void tearDown() {
        if (mTempFile.exists()) {
            Assert.assertTrue("Failed to delete \"" + mTempFile + "\"", mTempFile.delete());
        }
    }

    @Test
    @MediumTest
    // Test that the service saves metrics records to file
    public void testSaveToFile() throws Throwable {
        HistogramRecord recordBooleanProto = HistogramRecord.newBuilder()
                                                     .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                                                     .setHistogramName("testSaveToFile.boolean")
                                                     .setSample(1)
                                                     .build();
        HistogramRecord recordLinearProto = HistogramRecord.newBuilder()
                                                    .setRecordType(RecordType.HISTOGRAM_LINEAR)
                                                    .setHistogramName("testSaveToFile.linear")
                                                    .setSample(123)
                                                    .setMin(1)
                                                    .setMax(1000)
                                                    .setNumBuckets(50)
                                                    .build();
        HistogramRecordList expectedListProto = HistogramRecordList.newBuilder()
                                                        .addRecords(recordBooleanProto)
                                                        .addRecords(recordLinearProto)
                                                        .addRecords(recordBooleanProto)
                                                        .build();

        // Cannot bind to service using real connection since we need to inject test file name.
        MetricsBridgeService service = new MetricsBridgeService(mTempFile);
        // Simulate starting the service by calling onCreate()
        service.onCreate();

        IBinder binder = service.onBind(null);
        IMetricsBridgeService stub = IMetricsBridgeService.Stub.asInterface(binder);
        stub.recordMetrics(recordBooleanProto.toByteArray());
        stub.recordMetrics(recordLinearProto.toByteArray());
        stub.recordMetrics(recordBooleanProto.toByteArray());

        // Block until all tasks are finished to make sure all records are written to file.
        FutureTask<Object> blockTask = service.addTaskToBlock();
        AwActivityTestRule.waitForFuture(blockTask);

        HistogramRecordList resultListProto = readProtoFromFile(mTempFile);
        Assert.assertEquals(
                "constructed list proto from file is different from the expected list proto",
                resultListProto, expectedListProto);
    }

    @Test
    @MediumTest
    // Test that service recovers saved data from file, appends new records to it and
    // clears the file after a retrieve call.
    public void testRetrieveFromFile() throws Throwable {
        HistogramRecord recordBooleanProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testRecoverFromFile.boolean")
                        .setSample(1)
                        .build();
        HistogramRecord recordLinearProto = HistogramRecord.newBuilder()
                                                    .setRecordType(RecordType.HISTOGRAM_LINEAR)
                                                    .setHistogramName("testRecoverFromFile.linear")
                                                    .setSample(123)
                                                    .setMin(1)
                                                    .setMax(1000)
                                                    .setNumBuckets(50)
                                                    .build();
        HistogramRecordList initialListProto = HistogramRecordList.newBuilder()
                                                       .addRecords(recordBooleanProto)
                                                       .addRecords(recordLinearProto)
                                                       .addRecords(recordBooleanProto)
                                                       .build();
        HistogramRecordList expectedListProto =
                initialListProto.toBuilder().addRecords(recordBooleanProto).build();
        writeProtoToFile(initialListProto, mTempFile);

        // Cannot bind to service using real connection since we need to inject test file name.
        MetricsBridgeService service = new MetricsBridgeService(mTempFile);
        // Simulate starting the service by calling onCreate()
        service.onCreate();

        IBinder binder = service.onBind(null);
        IMetricsBridgeService stub = IMetricsBridgeService.Stub.asInterface(binder);
        stub.recordMetrics(recordBooleanProto.toByteArray());
        byte[] retrievedData = stub.retrieveNonembeddedMetrics();

        // Assert file is deleted after the retrieve call
        Assert.assertFalse(
                "file should be deleted after retrieve metrics call", mTempFile.exists());
        Assert.assertNotNull("retrieved byte data from the service is null", retrievedData);
        Assert.assertArrayEquals("retrieved byte data is different from the expected data",
                expectedListProto.toByteArray(), retrievedData);
    }

    @Test
    @MediumTest
    // Test sending data to the service and retrieving it back.
    public void testRecordAndRetrieveNonembeddedMetrics() {
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testRecordAndRetrieveNonembeddedMetrics")
                        .setSample(1)
                        .build();
        HistogramRecordList expectedListProto =
                HistogramRecordList.newBuilder().addRecords(recordProto).build();

        testRecordMetrics(recordProto.toByteArray(), false);
        byte[] retrievedData = testRetrieveNonembeddedMetrics(true);

        Assert.assertNotNull("retrieved byte data from the service is null", retrievedData);
        Assert.assertArrayEquals("retrieved byte data is different from the expected data",
                expectedListProto.toByteArray(), retrievedData);
    }

    @Test
    @MediumTest
    // Test sending data to the service and retrieving it back and make sure it's cleared.
    public void testClearAfterRetrieveNonembeddedMetrics() {
        HistogramRecord recordProto =
                HistogramRecord.newBuilder()
                        .setRecordType(RecordType.HISTOGRAM_BOOLEAN)
                        .setHistogramName("testClearAfterRetrieveNonembeddedMetrics")
                        .setSample(1)
                        .build();
        HistogramRecordList expectedListProto =
                HistogramRecordList.newBuilder().addRecords(recordProto).build();

        testRecordMetrics(recordProto.toByteArray(), false);
        byte[] retrievedData = testRetrieveNonembeddedMetrics(false);

        Assert.assertNotNull("retrieved byte data from the service is null", retrievedData);
        Assert.assertArrayEquals("retrieved byte data is different from the expected data",
                expectedListProto.toByteArray(), retrievedData);

        // Retrieve data a second time to make sure it has been cleared after the first call
        retrievedData = testRetrieveNonembeddedMetrics(true);

        Assert.assertTrue(
                "metrics kept by the service hasn't been cleared", retrievedData.length == 0);
    }

    private static void writeProtoToFile(HistogramRecordList recordList, File file)
            throws IOException {
        FileOutputStream out = new FileOutputStream(file);
        for (HistogramRecord record : recordList.getRecordsList()) {
            record.writeDelimitedTo(out);
        }
        out.close();
    }

    private static HistogramRecordList readProtoFromFile(File file)
            throws IOException, InvalidProtocolBufferException {
        FileInputStream in = new FileInputStream(file);
        HistogramRecordList.Builder listBuilder = HistogramRecordList.newBuilder();
        HistogramRecord savedProto = HistogramRecord.parseDelimitedFrom(in);
        while (savedProto != null) {
            listBuilder.addRecords(savedProto);
            savedProto = HistogramRecord.parseDelimitedFrom(in);
        }
        in.close();
        return listBuilder.build();
    }

    // Connect to MetricsBridgeService and call recordMetrics with the given data, assert that
    // the method is called and returned.
    private void testRecordMetrics(byte[] data, boolean unbind) {
        final ConditionVariable recordMetricsCalled = new ConditionVariable();

        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                try {
                    IMetricsBridgeService.Stub.asInterface(service).recordMetrics(data);
                } catch (RemoteException e) {
                    Assert.fail("Faild recording a histogram: " + e.getMessage());
                } finally {
                    if (unbind) {
                        ContextUtils.getApplicationContext().unbindService(this);
                    }
                    recordMetricsCalled.open();
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsBridgeService.class);
        Assert.assertTrue("Failed to bind to MetricsBridgeService",
                ContextUtils.getApplicationContext().bindService(
                        intent, connection, Context.BIND_AUTO_CREATE));
        Assert.assertTrue("Timed out waiting for recordMetrics() to return",
                recordMetricsCalled.block(BINDER_TIMEOUT_MILLIS));
    }

    /**
     * A Service connection class that connect to MetricsBridgeService to retrieve recorded metrics
     * data and provide a timed blocking call to access it.
     */
    private static class TestRetrieveConnection implements ServiceConnection {
        private byte[] mData;
        private final ConditionVariable mMethodCalled = new ConditionVariable();
        private final boolean mUnbind;

        public TestRetrieveConnection(boolean unbind) {
            mUnbind = unbind;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                mData = IMetricsBridgeService.Stub.asInterface(service)
                                .retrieveNonembeddedMetrics();
                Assert.assertNotNull("retrieved byte data from the service is null", mData);
            } catch (RemoteException e) {
                Assert.fail("Faild retrieving metrics: " + e.getMessage());
            } finally {
                if (mUnbind) {
                    ContextUtils.getApplicationContext().unbindService(this);
                }
                mMethodCalled.open();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}

        // Block until the onServiceConnected is called and data is retrieveNonembeddedMetrics is
        // called, fail otherwise.
        public byte[] getData() {
            Assert.assertTrue("Timed out waiting for retrieveNonembeddedMetrics() to return",
                    mMethodCalled.block(BINDER_TIMEOUT_MILLIS));
            return mData;
        }
    }

    private byte[] testRetrieveNonembeddedMetrics(boolean unbind) {
        TestRetrieveConnection connection = new TestRetrieveConnection(unbind);
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MetricsBridgeService.class);
        Assert.assertTrue("Failed to bind to MetricsBridgeService",
                ContextUtils.getApplicationContext().bindService(
                        intent, connection, Context.BIND_AUTO_CREATE));

        return connection.getData();
    }
}
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

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord.RecordType;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecordList;
import org.chromium.android_webview.services.MetricsBridgeService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;

/**
 * Test MetricsBridgeService.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
// TODO(hazems) test writing to the file
public class MetricsBridgeServiceTest {
    private static final long BINDER_TIMEOUT_MILLIS = 10000;

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
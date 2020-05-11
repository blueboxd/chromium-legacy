// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.app.Service;
import android.content.Intent;
import android.os.ConditionVariable;
import android.os.IBinder;

import org.junit.Assert;

import org.chromium.android_webview.common.services.IMetricsBridgeService;

/**
 * Mock implementation for MetricsBridgeService that record metrics data and provide methods
 * for testing this data. Used in
 * {@link org.chromium.android_webview.test.common.metrics.AwNonembeddedUmaRecorderTest}.
 */
public class MockMetricsBridgeService extends Service {
    public static final long BINDER_TIMEOUT_MILLIS = 10000;

    private static byte[] sRecordedData;
    private static final ConditionVariable sMethodCalled = new ConditionVariable();

    private final IMetricsBridgeService.Stub mMockBinder = new IMetricsBridgeService.Stub() {
        @Override
        public void recordMetrics(byte[] data) {
            sRecordedData = data;
            sMethodCalled.open();
        }

        @Override
        public byte[] retrieveNonembeddedMetrics() {
            throw new UnsupportedOperationException();
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mMockBinder;
    }

    /**
     * Block until the recordMetrics is called and data is set is fail after
     * {@link BINDER_TIMEOUT_MILLIS}.
     *
     * @return byte date received by recordMetrics.
     */
    public static byte[] getRecordedData() {
        Assert.assertTrue("Timed out waiting for recordMetrics() to be called",
                sMethodCalled.block(BINDER_TIMEOUT_MILLIS));
        return sRecordedData;
    }
}
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;

/**
 * Tests for memory_metrics_logger.cc.
 */
@JNINamespace("android_webview")
@RunWith(AwJUnit4ClassRunner.class)
public class MemoryMetricsLoggerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Before
    public void setUp() throws Exception {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);

        mActivityTestRule.loadUrlSync(testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(), "about:blank");
        Assert.assertTrue(nativeForceRecordHistograms());
    }

    @After
    public void tearDown() {}

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @SmallTest
    public void testMultiProcessHistograms() {
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Browser.PrivateMemoryFootprint"));
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Renderer.PrivateMemoryFootprint"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(SINGLE_PROCESS)
    @SmallTest
    public void testSingleProcessHistograms() {
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Browser.PrivateMemoryFootprint"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Renderer.PrivateMemoryFootprint"));
    }

    /**
     * Calls to MemoryMetricsLogger to force recording histograms, returning true on success.
     * A value of false means recording failed (most likely because process metrics not available.
     */
    public static native boolean nativeForceRecordHistograms();
}

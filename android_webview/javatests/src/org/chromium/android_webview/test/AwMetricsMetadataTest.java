// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Integration test to verify WebView's metrics metadata. This isn't a great spot to verify WebView
 * reports metadata correctly; that should be done with unittests on individual MetricsProviders.
 * This is an opportunity to verify these MetricsProviders are hooked up in WebView's
 * implementation.
 *
 * <p>This configures the initial metrics upload to happen very quickly (so tests don't need to run
 * multiple seconds). This does not touch the upload interval between subsequent uploads, because
 * each test runs in a separate browser process, so we'll never reach subsequent uploads. See
 * https://crbug.com/932582.
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING}) // Override sampling logic
public class AwMetricsMetadataTest {
    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestPlatformServiceBridge mPlatformServiceBridge;

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private final BlockingQueue<byte[]> mQueue;

        public TestPlatformServiceBridge() {
            mQueue = new LinkedBlockingQueue<>();
        }

        @Override
        public boolean canUseGms() {
            return true;
        }

        @Override
        public void queryMetricsSetting(Callback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            callback.onResult(true /* enabled */);
        }

        @Override
        public void logMetrics(byte[] data) {
            mQueue.add(data);
        }

        /**
         * Gets the latest metrics log we've received.
         */
        public ChromeUserMetricsExtension waitForNextMetricsLog() throws Exception {
            byte[] data = AwActivityTestRule.waitForNextQueueElement(mQueue);
            return ChromeUserMetricsExtension.parseFrom(data);
        }
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        // Kick off the metrics consent-fetching process. TestPlatformServiceBridge mocks out user
        // consent for when we query it with AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(),
        // so metrics consent is guaranteed to be granted.
        mPlatformServiceBridge = new TestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Need to configure the metrics delay first, because
            // handleMinidumpsAndSetMetricsConsent() triggers MetricsService initialization.
            AwMetricsServiceClient.setFastStartupForTesting(true);
            AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(true /* updateMetricsConsent */);
        });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_basicInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(ChromeUserMetricsExtension.Product.ANDROID_WEBVIEW,
                ChromeUserMetricsExtension.Product.forNumber(log.getProduct()));
        Assert.assertTrue("Should have some client_id", log.hasClientId());
        Assert.assertTrue("Should have some session_id", log.hasSessionId());
    }
}

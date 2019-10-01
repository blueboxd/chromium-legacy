// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.net.test.util.TestWebServer;

/**
 * Integration test for PageLoadMetrics.
 */
public class AwPageLoadMetricsTest {
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private void loadUrlSync(String url) throws Exception {
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    /**
     * This test doesn't intent to cover all UseCounter metrics, and just test WebView integration
     * works
     */
    public void testUseCounterMetrics() throws Throwable {
        final String data = "<html><head></head><body><form></form></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        MetricsUtils.HistogramDelta delta = new MetricsUtils.HistogramDelta(
                "Blink.UseCounter.MainFrame.Features", WebFeature.PAGE_VISITS);
        MetricsUtils.HistogramDelta form = new MetricsUtils.HistogramDelta(
                "Blink.UseCounter.Features", WebFeature.FORM_ELEMENT);
        loadUrlSync(url);
        loadUrlSync("about:blank");
        assertEquals(1, delta.getDelta());
        assertEquals(1, form.getDelta());
    }
}

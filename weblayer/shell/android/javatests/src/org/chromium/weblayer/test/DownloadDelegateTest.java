// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.DownloadDelegate;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests that the DownloadDelegate method is invoked for downloads.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DownloadDelegateTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    private WebLayerShellActivity mActivity;
    private Delegate mDelegate;

    private static class Delegate extends DownloadDelegate {
        public String mUrl;
        public String mUserAgent;
        public String mContentDisposition;
        public String mMimetype;
        public long mContentLength;

        @Override
        public void downloadRequested(String url, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimetype = mimetype;
            mContentLength = contentLength;
        }

        public void waitForDownload() {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mUrl != null;
                }
            }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl(null);
        Assert.assertNotNull(mActivity);

        mDelegate = new Delegate();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowserController().setDownloadDelegate(mDelegate); });
    }

    /**
     * Verifies the DownloadDelegate is informed of downloads resulting from navigations to pages
     * with Content-Disposition attachment.
     */
    @Test
    @SmallTest
    public void testDownloadByContentDisposition() throws Throwable {
        final String data = "download data";
        final String contentDisposition = "attachment;filename=\"download.txt\"";
        final String mimetype = "text/plain";

        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimetype));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse("/download.txt", data, downloadHeaders);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mActivity.getBrowserController().getNavigationController().navigate(
                        Uri.parse(pageUrl));
            });
            mDelegate.waitForDownload();

            Assert.assertEquals(pageUrl, mDelegate.mUrl);
            Assert.assertEquals(contentDisposition, mDelegate.mContentDisposition);
            Assert.assertEquals(mimetype, mDelegate.mMimetype);
            Assert.assertEquals(data.length(), mDelegate.mContentLength);
            // TODO(estade): verify mUserAgent.
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Verifies the DownloadDelegate is informed of downloads resulting from the user clicking on a
     * download link.
     */
    @Test
    @SmallTest
    public void testDownloadByLinkAttribute() {
        EmbeddedTestServer testServer = new EmbeddedTestServer();

        testServer.initializeNative(InstrumentationRegistry.getInstrumentation().getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        testServer.addDefaultHandlers("weblayer/test/data");
        Assert.assertTrue(testServer.start(0));

        String pageUrl = testServer.getURL("/download.html");
        mActivityTestRule.navigateAndWait(pageUrl);

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForDownload();
        Assert.assertEquals(testServer.getURL("/lorem_ipsum.txt"), mDelegate.mUrl);

        testServer.stopAndDestroyServer();
    }
}

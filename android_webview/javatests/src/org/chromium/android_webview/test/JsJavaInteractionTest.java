// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedTitleHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for JavaScript Java interaction.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class JsJavaInteractionTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String RESOURCE_PATH = "/android_webview/test/data";
    private static final String POST_MESSAGE_SIMPLE_HTML =
            RESOURCE_PATH + "/post_message_simple.html";
    private static final String POST_MESSAGE_WITH_PORTS_HTML =
            RESOURCE_PATH + "/post_message_with_ports.html";
    private static final String POST_MESSAGE_REPEAT_HTML =
            RESOURCE_PATH + "/post_message_repeat.html";

    private static final String HELLO = "Hello";
    private static final String NEW_TITLE = "new_title";
    private static final String JS_OBJECT_NAME = "myObject";
    private static final String DATA_HTML = "<html><body>data</body></html>";
    private static final int MESSAGE_COUNT = 10000;

    // Timeout to failure, in milliseconds
    private static final long TIMEOUT = scaleTimeout(5000);

    private EmbeddedTestServer mTestServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebMessageListener mListener;

    private static class TestWebMessageListener implements WebMessageListener {
        private LinkedBlockingQueue<Data> mQueue = new LinkedBlockingQueue<>();

        public static class Data {
            public AwContents mAwContents;
            public String mMessage;
            public Uri mSourceOrigin;
            public boolean mIsMainFrame;
            public MessagePort mReplyPort;
            public MessagePort[] mPorts;

            public Data(AwContents awContents, String message, Uri sourceOrigin,
                    boolean isMainFrame, MessagePort replyPort, MessagePort[] ports) {
                mAwContents = awContents;
                mMessage = message;
                mSourceOrigin = sourceOrigin;
                mIsMainFrame = isMainFrame;
                mReplyPort = replyPort;
                mPorts = ports;
            }
        }

        @Override
        public void onPostMessage(AwContents awContents, String message, Uri sourceOrigin,
                boolean isMainFrame, MessagePort replyPort, MessagePort[] ports) {
            mQueue.add(new Data(awContents, message, sourceOrigin, isMainFrame, replyPort, ports));
        }

        public Data waitForOnPostMessage() throws Exception {
            return waitForNextQueueElement(mQueue);
        }

        public boolean hasNoMoreOnPostMessage() {
            return mQueue.isEmpty();
        }
    }

    private static <T> T waitForNextQueueElement(LinkedBlockingQueue<T> queue) throws Exception {
        T value = queue.poll(TIMEOUT, TimeUnit.MILLISECONDS);
        if (value == null) {
            throw new TimeoutException(
                    "Timeout while trying to take next entry from BlockingQueue");
        }
        return value;
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mListener = new TestWebMessageListener();
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageSimple() throws Throwable {
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        final String url = loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.mMessage);
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageWithPorts() throws Throwable {
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        final String url = loadUrlFromPath(POST_MESSAGE_WITH_PORTS_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        final MessagePort[] ports = data.mPorts;
        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.mMessage);
        Assert.assertEquals(1, ports.length);

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage on
        // this port succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        ports[0].postMessage(NEW_TITLE, new MessagePort[0]);
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageRepeated() throws Throwable {
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        final String url = loadUrlFromPath(POST_MESSAGE_REPEAT_HTML);
        for (int i = 0; i < MESSAGE_COUNT; ++i) {
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
            assertUrlHasOrigin(url, data.mSourceOrigin);
            Assert.assertEquals(HELLO + ":" + i, data.mMessage);
        }

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageFromIframeWorks() throws Throwable {
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        final String frameUrl = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);

        // Load a cross origin iframe page.
        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), html, "text/html", false,
                "http://www.google.com", null);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(frameUrl, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.mMessage);
        Assert.assertFalse(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testSetWebMessageListenerAfterPageLoadWontAffectCurrentPage() throws Throwable {
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Set WebMessageListener after the page loaded won't affect the current page.
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // We shouldn't have executed postMessage on JS_OBJECT_NAME either.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testSetWebMessageListenerAffectsRendererInitiatedNavigation() throws Throwable {
        // TODO(crbug.com/969842): We'd either replace the following html file with a file contains
        // no JavaScript code or add a test to ensure that evaluateJavascript() won't
        // over-trigger DidClearWindowObject.
        loadUrlFromPath(POST_MESSAGE_WITH_PORTS_HTML);

        // Set WebMessageListener after the page loaded.
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Navigate to a different web page from renderer and wait until the page loading finished.
        final String url = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        final int currentCallCount = onPageFinishedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mAwContents.evaluateJavaScriptForTests(
                                "window.location.href = '" + url + "';", null));
        onPageFinishedHelper.waitForCallback(currentCallCount);

        // We should expect one onPostMessage event.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.mMessage);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testSetWebMessageListenerWontAffectOtherAwContents() throws Throwable {
        // Create another AwContents object.
        final TestAwContentsClient awContentsClient = new TestAwContentsClient();
        final AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(awContentsClient);
        final AwContents otherAwContents = awTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(otherAwContents);

        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        final String url = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        mActivityTestRule.loadUrlSync(
                otherAwContents, awContentsClient.getOnPageFinishedHelper(), url);

        // Verify that WebMessageListener was set successfually on mAwContents.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.mMessage);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Verify that we don't have myObject injected to otherAwContents.
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, otherAwContents, awContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testSetWebMessageListenerAllowsCertainUrlWorksWithIframe() throws Throwable {
        final String frameUrl = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);
        setWebMessageListenerOnUiThread(
                mAwContents, mListener, new String[] {parseOrigin(frameUrl)});

        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), html, "text/html", false,
                "http://www.google.com", null);

        // The iframe should have myObject injected.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.mMessage);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Verify that the main frame has no myObject injected.
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testRemoveWebMessageListenerCouldPreventInjectionForNextPageLoad()
            throws Throwable {
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        // Load the the page.
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.mMessage);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove WebMessageListener will disable injection for next page load.
        TestThreadUtils.runOnUiThreadBlocking(() -> mAwContents.removeWebMessageListener());

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Should have no myObject injected.
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAllowedOriginsWorksForVariousBaseUrls() throws Throwable {
        // Set a typical rule.
        setWebMessageListenerOnUiThread(
                mAwContents, mListener, new String[] {"https://www.example.com:443"});

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("http://www.example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("http://example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.google.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("file://etc"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("ftp://example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl(null));

        // Inject to all frames.
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"*"});

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("http://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("http://example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.google.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("ftp://example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl(null));

        // ftp scheme.
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"ftp://example.com"});
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("ftp://example.com"));

        // file scheme.
        setWebMessageListenerOnUiThread(mAwContents, mListener, new String[] {"file://*"});
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc"));

        // Pass an URI instead of origin shouldn't work.
        try {
            setWebMessageListenerOnUiThread(
                    mAwContents, mListener, new String[] {"https://www.example.com/index.html"});
            Assert.fail("allowedOriginRules shouldn't be url like");
        } catch (RuntimeException e) {
            // Should catch IllegalArgumentException in the end of the re-throw chain.
            Throwable ex = e;
            while (ex.getCause() != null) {
                ex = ex.getCause();
            }
            Assert.assertTrue(ex instanceof IllegalArgumentException);
        }
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testCallSetWebMessageListenerAgainForTheSameOrigins() throws Throwable {
        final String[] allowedOriginRules =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules);

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));

        // Call setWebMessageListener() with the same set of origins.
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules);

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testCallSetWebMessageListenerAgainForOverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules1);

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));

        final String[] allowedOriginRules2 =
                new String[] {"https://www.example.com", "https://www.ok.com"};
        // Call setWebMessageListener with overlapping set of origins.
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules2);

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testCallSetWebMessageListenerAgainForNonOverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules1);

        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));

        final String[] allowedOriginRules2 = new String[] {"https://www.ok.com"};
        // Call setWebMessageListener with non-overlapping set of origins.
        setWebMessageListenerOnUiThread(mAwContents, mListener, allowedOriginRules2);

        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.example.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com"));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com"));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl(""));
    }

    private boolean isJsObjectInjectedWhenLoadingUrl(final String baseUrl) throws Throwable {
        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), DATA_HTML, "text/html", false, baseUrl,
                null);
        return hasJavaScriptObject(JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient);
    }

    private String loadUrlFromPath(String path) throws Exception {
        final String url = mTestServer.getURL(path);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        return url;
    }

    private static void assertUrlHasOrigin(final String url, final Uri origin) {
        Uri uriFromServer = Uri.parse(url);
        Assert.assertEquals(uriFromServer.getScheme(), origin.getScheme());
        Assert.assertEquals(uriFromServer.getHost(), origin.getHost());
        Assert.assertEquals(uriFromServer.getPort(), origin.getPort());
    }

    private static String createCrossOriginAccessTestPageHtml(final String frameUrl) {
        return "<html>"
                + "<body><div>I have an iframe</ div>"
                + "  <iframe src ='" + frameUrl + "'></iframe>"
                + "</body></html>";
    }

    private static void setWebMessageListenerOnUiThread(final AwContents awContents,
            final WebMessageListener listener, final String[] allowedOriginRules) {
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> awContents.setWebMessageListener(listener,
                                                              JS_OBJECT_NAME, allowedOriginRules));
    }

    private static boolean hasJavaScriptObject(final String jsObjectName,
            final AwActivityTestRule rule, final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        final String result = rule.executeJavaScriptAndWaitForResult(
                awContents, contentsClient, "typeof " + jsObjectName + " !== 'undefined'");
        return result.equals("true");
    }

    private static String parseOrigin(String url) {
        final Uri uri = Uri.parse(url);
        final StringBuilder sb = new StringBuilder();
        final String scheme = uri.getScheme();
        final int port = uri.getPort();

        if (!scheme.isEmpty()) {
            sb.append(scheme);
            sb.append("://");
        }
        sb.append(uri.getHost());
        if (port != -1) {
            sb.append(":");
            sb.append(port);
        }
        return sb.toString();
    }
}

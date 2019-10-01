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
import org.chromium.android_webview.JsReplyProxy;
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
    private static final String POST_MESSAGE_REPLY_HTML =
            RESOURCE_PATH + "/post_message_receives_reply.html";

    private static final String HELLO = "Hello";
    private static final String NEW_TITLE = "new_title";
    private static final String JS_OBJECT_NAME = "myObject";
    private static final String JS_OBJECT_NAME_2 = "myObject2";
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
            public String mMessage;
            public Uri mSourceOrigin;
            public boolean mIsMainFrame;
            public JsReplyProxy mReplyProxy;
            public MessagePort[] mPorts;

            public Data(String message, Uri sourceOrigin, boolean isMainFrame,
                    JsReplyProxy replyProxy, MessagePort[] ports) {
                mMessage = message;
                mSourceOrigin = sourceOrigin;
                mIsMainFrame = isMainFrame;
                mReplyProxy = replyProxy;
                mPorts = ports;
            }
        }

        @Override
        public void onPostMessage(String message, Uri sourceOrigin, boolean isMainFrame,
                JsReplyProxy replyProxy, MessagePort[] ports) {
            mQueue.add(new Data(message, sourceOrigin, isMainFrame, replyProxy, ports));
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
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageSimple() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
    public void testAddWebMessageListenerAfterPageLoadWontAffectCurrentPage() throws Throwable {
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Add WebMessageListener after the page loaded won't affect the current page.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // We shouldn't have executed postMessage on JS_OBJECT_NAME either.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAddTheSameWebMessageListenerForDifferentJsObjectsWorks() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.mMessage);

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME_2 + ".postMessage('" + HELLO + "');");

        TestWebMessageListener.Data data2 = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data2.mMessage);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testFragmentNavigationWontDoJsInjection() throws Throwable {
        String url = loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Add WebMessageListener after the page loaded won't affect the current page.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load with fragment url.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), url + "#fragment");

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // We shouldn't have executed postMessage on JS_OBJECT_NAME either.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAddWebMessageListenerAffectsRendererInitiatedNavigation() throws Throwable {
        // TODO(crbug.com/969842): We'd either replace the following html file with a file contains
        // no JavaScript code or add a test to ensure that evaluateJavascript() won't
        // over-trigger DidClearWindowObject.
        loadUrlFromPath(POST_MESSAGE_WITH_PORTS_HTML);

        // Add WebMessageListener after the page loaded.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
    public void testAddWebMessageListenerWontAffectOtherAwContents() throws Throwable {
        // Create another AwContents object.
        final TestAwContentsClient awContentsClient = new TestAwContentsClient();
        final AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(awContentsClient);
        final AwContents otherAwContents = awTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(otherAwContents);

        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

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
    public void testAddWebMessageListenerAllowsCertainUrlWorksWithIframe() throws Throwable {
        final String frameUrl = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, new String[] {parseOrigin(frameUrl)}, mListener);

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
    public void testRemoveWebMessageListener_preventInjectionForNextPageLoad() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load the the page.
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.mMessage);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove WebMessageListener will disable injection for next page load.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Should have no myObject injected.
        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testRemoveWebMessageListener_cutJsJavaMappingImmediately() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load the the page.
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.mMessage);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove WebMessageListener will disable injection for next page load.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        // Should still have myObject.
        Assert.assertTrue(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // But posting message on myObject will be dropped.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME + ".postMessage('" + HELLO + "');");
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testRemoveWebMessageListener_removeWithNoAddWebMessageListener() throws Throwable {
        // Call removeWebMessageListener() without addWebMessageListener() shouldn't fail.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testRemoveWebMessageListener_removeBeforeLoadPage() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testRemoveWebMessageListener_extraRemove() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);
        // Extra removeWebMessageListener() does nothing.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        Assert.assertFalse(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAllowedOriginsWorksForVariousBaseUrls() throws Throwable {
        // Set a typical rule.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME,
                new String[] {"https://www.example.com:443"}, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("http://www.example.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("http://example.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.google.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("file://etc", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl(null, JS_OBJECT_NAME));

        // Inject to all frames.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("http://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("http://example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.google.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl(null, JS_OBJECT_NAME_2));

        // ftp scheme.
        final String jsObjectName3 = JS_OBJECT_NAME + "3";
        addWebMessageListenerOnUiThread(
                mAwContents, jsObjectName3, new String[] {"ftp://example.com"}, mListener);
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", jsObjectName3));

        // file scheme.
        final String jsObjectName4 = JS_OBJECT_NAME + "4";
        addWebMessageListenerOnUiThread(
                mAwContents, jsObjectName4, new String[] {"file://*"}, mListener);
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc", jsObjectName4));

        // Pass an URI instead of origin shouldn't work.
        final String jsObjectName5 = JS_OBJECT_NAME + "5";
        try {
            addWebMessageListenerOnUiThread(mAwContents, jsObjectName5,
                    new String[] {"https://www.example.com/index.html"}, mListener);
            Assert.fail("allowedOriginRules shouldn't be url like");
        } catch (RuntimeException e) {
            // Should catch IllegalArgumentException in the end of the re-throw chain.
            Throwable ex = e;
            while (ex.getCause() != null) {
                ex = ex.getCause();
            }
            Assert.assertTrue(ex instanceof IllegalArgumentException);
        }
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", jsObjectName5));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testDontAllowAddWebMessageLitenerWithTheSameJsObjectName() {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        try {
            addWebMessageListenerOnUiThread(
                    mAwContents, JS_OBJECT_NAME, new String[] {"*"}, new TestWebMessageListener());
            Assert.fail("Shouldn't allow the same Js object name be added more than once.");
        } catch (RuntimeException e) {
            // Should catch IllegalArgumentException in the end of the re-throw chain.
            Throwable ex = e;
            while (ex.getCause() != null) {
                ex = ex.getCause();
            }
            Assert.assertTrue(ex instanceof IllegalArgumentException);
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAddWebMessageListener_SameOrigins() throws Throwable {
        final String[] allowedOriginRules =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, allowedOriginRules, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        // Call addWebMessageListener() with the same set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAddWebMessageListener_OverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, allowedOriginRules1, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        final String[] allowedOriginRules2 =
                new String[] {"https://www.example.com", "https://www.ok.com"};
        // Call addWebMessageListener with overlapping set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules2, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));

        // Remove the listener should remove the js object from the next navigation.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME_2);

        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testAddWebMessageListener_NonOverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, allowedOriginRules1, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        final String[] allowedOriginRules2 = new String[] {"https://www.ok.com"};
        // Call addWebMessageListener with non-overlapping set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules2, mListener);

        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testJsReplyProxyWorks() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage to proxy
        // succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        data.mReplyProxy.postMessage(NEW_TITLE);
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testJsReplyProxyReplyToTheCorrectJsObject() throws Throwable {
        final TestWebMessageListener webMessageListener2 = new TestWebMessageListener();
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, webMessageListener2);

        final String url = loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        // Listener for myObject.
        final String listener1 = "function (event) {"
                + "  if (window.listenerResult1) {"
                + "    window.listenerResult1 += event.data;"
                + "  } else {"
                + "    window.listenerResult1 = event.data;"
                + "  }"
                + "}";

        // Listener for myObject2.
        final String listener2 = "function (event) {"
                + "  if (window.listenerResult2) {"
                + "    window.listenerResult2 += event.data;"
                + "  } else {"
                + "    window.listenerResult2 = event.data;"
                + "  }"
                + "}";

        // Add two different js objects.
        addEventListener(listener1, "listener1", JS_OBJECT_NAME, mActivityTestRule, mAwContents,
                mContentsClient);
        addEventListener(listener2, "listener2", JS_OBJECT_NAME_2, mActivityTestRule, mAwContents,
                mContentsClient);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        final String message = "message";
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                JS_OBJECT_NAME_2 + ".postMessage('" + message + "');");
        TestWebMessageListener.Data data2 = webMessageListener2.waitForOnPostMessage();

        Assert.assertEquals(message, data2.mMessage);

        final String hello1 = "hello1";
        final String hello2 = "hello2";
        // Targeting myObject.
        data.mReplyProxy.postMessage(hello1);
        // Targeting myObject2.
        data2.mReplyProxy.postMessage(hello2);

        Assert.assertEquals(hello1,
                getJsObjectValue(
                        "window.listenerResult1", mActivityTestRule, mAwContents, mContentsClient));
        Assert.assertEquals(hello2,
                getJsObjectValue(
                        "window.listenerResult2", mActivityTestRule, mAwContents, mContentsClient));

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
        Assert.assertTrue(webMessageListener2.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsReplyProxyDropsMessageIfJsObjectIsGone() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        JsReplyProxy proxy = data.mReplyProxy;

        // Load the same url again.
        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);
        TestWebMessageListener.Data data2 = mListener.waitForOnPostMessage();

        // Use the previous JsReplyProxy to send message. It should drop the message.
        proxy.postMessage(NEW_TITLE);

        // Call evaluateJavascript to make sure the previous postMessage() call is reached to
        // renderer if it should, since these messages are in sequence.
        Assert.assertTrue(hasJavaScriptObject(
                JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // Title shouldn't change.
        Assert.assertNotEquals(NEW_TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsAddAndRemoveEventListener() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        JsReplyProxy proxy = mListener.waitForOnPostMessage().mReplyProxy;

        final String listener1 = "function (event) {"
                + "  if (window.listenerResult1) {"
                + "    window.listenerResult1 += event.data;"
                + "  } else {"
                + "    window.listenerResult1 = event.data;"
                + "  }"
                + "}";

        final String listener2 = "function (event) {"
                + "  if (window.listenerResult2) {"
                + "    window.listenerResult2 += event.data;"
                + "  } else {"
                + "    window.listenerResult2 = event.data;"
                + "  }"
                + "}";

        addEventListener(listener1, "listener1", JS_OBJECT_NAME, mActivityTestRule, mAwContents,
                mContentsClient);
        addEventListener(listener2, "listener2", JS_OBJECT_NAME, mActivityTestRule, mAwContents,
                mContentsClient);

        // Post message to test both listeners receive message.
        final String message = "testListener";
        proxy.postMessage(message);

        Assert.assertEquals(message,
                getJsObjectValue(
                        "window.listenerResult1", mActivityTestRule, mAwContents, mContentsClient));
        Assert.assertEquals(message,
                getJsObjectValue(
                        "window.listenerResult2", mActivityTestRule, mAwContents, mContentsClient));

        removeEventListener(
                "listener2", JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient);

        // Post message again to test if remove works.
        proxy.postMessage(message);

        // listener 1 should add message again.
        Assert.assertEquals(message + message,
                getJsObjectValue(
                        "window.listenerResult1", mActivityTestRule, mAwContents, mContentsClient));
        // listener 2 result should remain the same.
        Assert.assertEquals(message,
                getJsObjectValue(
                        "window.listenerResult2", mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInterfaction"})
    public void testJsObjectRemoveOnMessage() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage to proxy
        // succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        data.mReplyProxy.postMessage(NEW_TITLE);
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME + ".onmessage = undefined;");
        Assert.assertEquals("null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, JS_OBJECT_NAME + ".onmessage"));

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private boolean isJsObjectInjectedWhenLoadingUrl(
            final String baseUrl, final String jsObjectName) throws Throwable {
        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), DATA_HTML, "text/html", false, baseUrl,
                null);
        return hasJavaScriptObject(jsObjectName, mActivityTestRule, mAwContents, mContentsClient);
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

    private static void addWebMessageListenerOnUiThread(final AwContents awContents,
            final String jsObjectName, final String[] allowedOriginRules,
            final WebMessageListener listener) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> awContents.addWebMessageListener(jsObjectName, allowedOriginRules, listener));
    }

    private static void removeWebMessageListenerOnUiThread(
            final AwContents awContents, final String jsObjectName) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> awContents.removeWebMessageListener(jsObjectName));
    }

    private static boolean hasJavaScriptObject(final String jsObjectName,
            final AwActivityTestRule rule, final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        final String result = rule.executeJavaScriptAndWaitForResult(
                awContents, contentsClient, "typeof " + jsObjectName + " !== 'undefined'");
        return result.equals("true");
    }

    private static void addEventListener(final String func, final String funcName,
            String jsObjectName, final AwActivityTestRule rule, final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        String code = "let " + funcName + " = " + func + ";";
        code += jsObjectName + ".addEventListener('message', " + funcName + ");";
        rule.executeJavaScriptAndWaitForResult(awContents, contentsClient, code);
    }

    private static void removeEventListener(final String funcName, final String jsObjectName,
            final AwActivityTestRule rule, final AwContents awContents,
            final TestAwContentsClient contentsClient) throws Throwable {
        String code = jsObjectName + ".removeEventListener('message', " + funcName + ")";
        rule.executeJavaScriptAndWaitForResult(awContents, contentsClient, code);
    }

    private static String getJsObjectValue(final String jsObjectName, final AwActivityTestRule rule,
            final AwContents awContents, final TestAwContentsClient contentsClient)
            throws Throwable {
        return rule.maybeStripDoubleQuotes(
                rule.executeJavaScriptAndWaitForResult(awContents, contentsClient, jsObjectName));
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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSession;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetError;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for detached resource requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DetachedResourceRequestTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private CustomTabsConnection mConnection;
    private Context mContext;
    private EmbeddedTestServer mServer;

    private static final Uri ORIGIN = Uri.parse("http://cats.google.com");
    private static final int NET_OK = 0;

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mConnection = CustomTabsTestUtils.setUpConnection();
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
        TestThreadUtils.runOnUiThreadBlocking(OriginVerifier::clearCachedVerificationsForTesting);
    }

    @After
    public void tearDown() throws Exception {
        CustomTabsTestUtils.cleanupSessions(mConnection);
        if (mServer != null) mServer.stopAndDestroyServer();
        mServer = null;
    }

    @Test
    @SmallTest
    public void testCanDoParallelRequest() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String packageName = mContext.getPackageName();
            OriginVerifier.addVerificationOverride(packageName, new Origin(ORIGIN.toString()),
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(session, ORIGIN));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_RESOURCE_PREFETCH)
    public void testCanDoResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String packageName = mContext.getPackageName();
            OriginVerifier.addVerificationOverride(packageName, new Origin(ORIGIN.toString()),

                    CustomTabsService.RELATION_USE_AS_ORIGIN);
        });
        Intent intent = prepareIntentForResourcePrefetch(
                Arrays.asList(Uri.parse("https://foo.bar")), ORIGIN);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent)));

        CustomTabsTestUtils.warmUpAndWait();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent)));

        mConnection.mClientManager.setAllowResourcePrefetchForSession(session, true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(1, mConnection.maybePrefetchResources(session, intent)));
    }

    @Test
    @SmallTest
    public void testStartParallelRequestValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            int expected = CustomTabsConnection.ParallelRequestStatus.NO_REQUEST;
            HistogramDelta histogram =
                    new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            Assert.assertEquals(expected, mConnection.handleParallelRequest(session, new Intent()));
            Assert.assertEquals(1, histogram.getDelta());

            expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            Intent intent =
                    prepareIntent(Uri.parse("android-app://this.is.an.android.app"), ORIGIN);
            Assert.assertEquals("Should not allow android-app:// scheme", expected,
                    mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());

            expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse(""), ORIGIN);
            Assert.assertEquals("Should not allow an empty URL", expected,
                    mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());

            expected =
                    CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse("HTTPS://foo.bar"), Uri.parse("wrong://origin"));
            Assert.assertEquals("Should not allow an arbitrary origin", expected,
                    mConnection.handleParallelRequest(session, intent));

            expected = CustomTabsConnection.ParallelRequestStatus.SUCCESS;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse("HTTPS://foo.bar"), ORIGIN);
            Assert.assertEquals(expected, mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_RESOURCE_PREFETCH)
    public void testStartResourcePrefetchUrlsValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, new Intent()));

            ArrayList<Uri> urls = new ArrayList<>();
            Intent intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("android-app://this.is.an.android.app"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse(""));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("https://foo.bar"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(1, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("https://bar.foo"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(2, mConnection.maybePrefetchResources(session, intent));

            intent = prepareIntentForResourcePrefetch(urls, Uri.parse("wrong://origin"));
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanStartParallelRequest() throws Exception {
        testCanStartParallelRequest(true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanStartParallelRequestBeforeNative() throws Exception {
        testCanStartParallelRequest(false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testParallelRequestFailureCallback() throws Exception {
        Uri url = Uri.parse("http://request-url");
        int status =
                CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(url, status, 0);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);
        CustomTabsTestUtils.warmUpAndWait();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertEquals(status,
                    mConnection.handleParallelRequest(
                            session, prepareIntent(url, Uri.parse("http://not-the-right-origin"))));
        });
        customTabsCallback.waitForRequest(0, 1);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testParallelRequestCompletionFailureCallback() throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL("/close-socket"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(url,
                        CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                        Math.abs(NetError.ERR_EMPTY_RESPONSE));
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        CustomTabsTestUtils.warmUpAndWait();
        customTabsCallback.waitForRequest(0, 1);
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion(0, 1);
    }

    @Test
    @SmallTest
    public void testCanStartResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        final CallbackHelper cb = new CallbackHelper();
        // We expect one read per prefetched url.
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });

        List<Uri> urls = Arrays.asList(Uri.parse(mServer.getURL("/echo-raw?a=1")),
                Uri.parse(mServer.getURL("/echo-raw?a=2")),
                Uri.parse(mServer.getURL("/echo-raw?a=3")));
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertEquals(urls.size(),
                    mConnection.maybePrefetchResources(
                            session, prepareIntentForResourcePrefetch(urls, ORIGIN)));
        });
        cb.waitForCallback(0, urls.size());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanSetCookie() throws Exception {
        testCanSetCookie(true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanSetCookieBeforeNative() throws Exception {
        testCanSetCookie(false);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a main resource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingMainResource() throws Exception {
        testSafeBrowsingMainResource(true);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a subresource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingSubresource() throws Exception {
        testSafeBrowsingSubresource(true);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a main resource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingMainResourceBeforeNative() throws Exception {
        testSafeBrowsingMainResource(false);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a subresource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingSubresourceBeforeNative() throws Exception {
        testSafeBrowsingSubresource(false);
    }

    @Test
    @SmallTest
    public void testCanBlockThirdPartyCookies() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"None\"", content);
    }

    @Test
    @SmallTest
    public void testThirdPartyCookieBlockingAllowsFirstParty() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        final Uri origin = Uri.parse(new Origin(url).toString());
        CustomTabsSessionToken session = prepareSession(url);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, origin)));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testRepeatedIntents() throws Exception {
        mServer = EmbeddedTestServer.createAndStartServer(mContext);

        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        DetachedResourceRequestCheckCallback callback = new DetachedResourceRequestCheckCallback(
                url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(callback).session;

        Uri launchedUrl = Uri.parse(mServer.getURL("/echotitle"));
        Intent intent = (new CustomTabsIntent.Builder(session).build()).intent;
        intent.setComponent(new ComponentName(mContext, ChromeLauncherActivity.class));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(launchedUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, url);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, ORIGIN);

        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(mConnection.newSession(token));
        mConnection.mClientManager.setAllowParallelRequestForSession(token, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerificationOverride(mContext.getPackageName(),
                    new Origin(ORIGIN.toString()), CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(token, ORIGIN));
        });

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        callback.waitForRequest(0, 1);
        callback.waitForCompletion(0, 1);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        callback.waitForRequest(1, 1);
        callback.waitForCompletion(1, 1);
    }

    private void testCanStartParallelRequest(boolean afterNative) throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL("/echotitle"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        if (afterNative) CustomTabsTestUtils.warmUpAndWait();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();

        customTabsCallback.waitForRequest(0, 1);
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion(0, 1);
    }

    private void testCanSetCookie(boolean afterNative) throws Exception {
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);
        if (afterNative) CustomTabsTestUtils.warmUpAndWait();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));

        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();
        customTabsCallback.waitForRequest(0, 1);
        customTabsCallback.waitForCompletion(0, 1);

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    private void testSafeBrowsingMainResource(boolean afterNative) throws Exception {
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        CustomTabsSessionToken session = prepareSession();

        String cacheable = "/cachetime";
        CallbackHelper readFromSocketCallback =
                waitForDetachedRequest(session, cacheable, afterNative);
        Uri url = Uri.parse(mServer.getURL(cacheable));

        try {
            MockSafeBrowsingApiHandler.addMockResponse(
                    url.toString(), "{\"matches\":[{\"threat_type\":\"5\"}]}");

            Intent intent =
                    CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, url.toString());
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

            // TODO(carlosil): For now, we check the presence of an interstitial through the title
            // since isShowingInterstitialPage does not work with committed interstitials. Once we
            // fully migrate to committed interstitials, this should be changed to a more robust
            // check.
            CriteriaHelper.pollUiThread(
                    () -> tab.getWebContents().getTitle().equals("Security error"));

            // 1 read from the detached request, and 0 from the page load, as
            // the response comes from the cache, and SafeBrowsing blocks it.
            Assert.assertEquals(1, readFromSocketCallback.getCallCount());
        } finally {
            MockSafeBrowsingApiHandler.clearMockResponses();
        }
    }

    private void testSafeBrowsingSubresource(boolean afterNative) throws Exception {
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        CustomTabsSessionToken session = prepareSession();
        String cacheable = "/cachetime";
        waitForDetachedRequest(session, cacheable, afterNative);
        Uri url = Uri.parse(mServer.getURL(cacheable));

        try {
            MockSafeBrowsingApiHandler.addMockResponse(
                    url.toString(), "{\"matches\":[{\"threat_type\":\"5\"}]}");

            String pageUrl = mServer.getURL("/chrome/test/data/android/cacheable_subresource.html");
            Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, pageUrl);
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            WebContents webContents = tab.getWebContents();
            // Need to poll as the subresource request is async.
            CriteriaHelper.pollUiThread(() -> webContents.isShowingInterstitialPage());
        } finally {
            MockSafeBrowsingApiHandler.clearMockResponses();
        }
    }

    private CustomTabsSessionToken prepareSession() throws Exception {
        return prepareSession(ORIGIN, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin) throws Exception {
        return prepareSession(origin, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin, CustomTabsCallback callback)
            throws Exception {
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(callback).session;
        Intent intent = (new CustomTabsIntent.Builder(session)).build().intent;
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(mConnection.newSession(token));
        mConnection.mClientManager.setAllowParallelRequestForSession(token, true);
        mConnection.mClientManager.setAllowResourcePrefetchForSession(token, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerificationOverride(mContext.getPackageName(),
                    new Origin(origin.toString()), CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(token, origin));
        });
        return token;
    }

    private void setUpTestServerWithListener(EmbeddedTestServer.ConnectionListener listener)
            throws InterruptedException {
        mServer = new EmbeddedTestServer();
        final CallbackHelper readFromSocketCallback = new CallbackHelper();
        mServer.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.setConnectionListener(listener);
        mServer.addDefaultHandlers("");
        Assert.assertTrue(mServer.start());
    }

    private CallbackHelper waitForDetachedRequest(CustomTabsSessionToken session,
            String relativeUrl, boolean afterNative) throws InterruptedException, TimeoutException {
        // Count the number of times data is read from the socket.
        // We expect 1 for the detached request.
        // Cannot count connections as Chrome opens multiple sockets at page load time.
        CallbackHelper readFromSocketCallback = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                readFromSocketCallback.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL(relativeUrl));
        if (afterNative) CustomTabsTestUtils.warmUpAndWait();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();
        readFromSocketCallback.waitForCallback(0);
        return readFromSocketCallback;
    }

    private static Intent prepareIntent(Uri url, Uri referrer) {
        Intent intent = new Intent();
        intent.setData(Uri.parse("http://www.example.com"));
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, url);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, referrer);
        return intent;
    }

    private static Intent prepareIntentForResourcePrefetch(List<Uri> urls, Uri referrer) {
        Intent intent = new Intent();
        intent.setData(Uri.parse("http://www.example.com"));
        intent.putExtra(CustomTabsConnection.RESOURCE_PREFETCH_URL_LIST_KEY, new ArrayList<>(urls));
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, referrer);
        return intent;
    }

    private static class DetachedResourceRequestCheckCallback extends CustomTabsCallback {
        private final Uri mExpectedUrl;
        private final int mExpectedRequestStatus;
        private final int mExpectedFinalStatus;
        private final CallbackHelper mRequestedWaiter = new CallbackHelper();
        private final CallbackHelper mCompletionWaiter = new CallbackHelper();

        public DetachedResourceRequestCheckCallback(
                Uri expectedUrl, int expectedRequestStatus, int expectedFinalStatus) {
            super();
            mExpectedUrl = expectedUrl;
            mExpectedRequestStatus = expectedRequestStatus;
            mExpectedFinalStatus = expectedFinalStatus;
        }

        @Override
        public void extraCallback(String callbackName, Bundle args) {
            if (CustomTabsConnection.ON_DETACHED_REQUEST_REQUESTED.equals(callbackName)) {
                Uri url = args.getParcelable("url");
                int status = args.getInt("status");
                Assert.assertEquals(mExpectedUrl, url);
                Assert.assertEquals(mExpectedRequestStatus, status);
                mRequestedWaiter.notifyCalled();
            } else if (CustomTabsConnection.ON_DETACHED_REQUEST_COMPLETED.equals(callbackName)) {
                Uri url = args.getParcelable("url");
                int status = args.getInt("net_error");
                Assert.assertEquals(mExpectedUrl, url);
                Assert.assertEquals(mExpectedFinalStatus, status);
                mCompletionWaiter.notifyCalled();
            }
        }

        public void waitForRequest() throws InterruptedException, TimeoutException {
            mRequestedWaiter.waitForCallback();
        }

        public void waitForRequest(int currentCallCount, int numberOfCallsToWaitFor)
                throws InterruptedException, TimeoutException {
            mRequestedWaiter.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
        }

        public void waitForCompletion() throws InterruptedException, TimeoutException {
            mCompletionWaiter.waitForCallback();
        }

        public void waitForCompletion(int currentCallCount, int numberOfCallsToWaitFor)
                throws InterruptedException, TimeoutException {
            mCompletionWaiter.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
        }
    }
}

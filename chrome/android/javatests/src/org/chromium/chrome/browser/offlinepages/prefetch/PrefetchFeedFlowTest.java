// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.graphics.Bitmap;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.support.test.filters.MediumTest;
import android.util.Base64;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.TestNetworkClient;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.NetworkStatusListenerAndroid;
import org.chromium.components.gcm_driver.instance_id.FakeInstanceIDWithSubtype;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_pages.core.prefetch.proto.StatusOuterClass;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.test.util.WebServer;
import org.chromium.ui.test.util.UiDisableIf;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for Prefetch, using the Feed as the suggestion provider.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction({ChromeRestriction.RESTRICTION_TYPE_REQUIRES_TOUCH})
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrefetchFeedFlowTest implements WebServer.RequestHandler {
    private static final String TAG = "PrefetchFlowTest";
    private TestOfflinePageService mOPS = new TestOfflinePageService();
    private WebServer mServer;
    private Profile mProfile;
    private CallbackHelper mPageAddedHelper = new CallbackHelper();
    private List<OfflinePageItem> mAddedPages =
            Collections.synchronizedList(new ArrayList<OfflinePageItem>());

    /* Feed test data
     * This file contains test data for Feed so that suggestions can be populated. This test
     * attempts to assume as little as possible about this data, so that it can be changed in the
     * future. Note: There are many other suggested URLs in this file, and URLs can even be
     * repeated.
     */
    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/feed_large.gcl.bin");
    // The first suggestion URL, thumbnail URL, and title.
    private static final String URL1 =
            "http://profootballtalk.nbcsports.com/2017/11/10/jerry-jones-owners-should-approve-of-roger-goodells-decisions/";
    private static final String THUMBNAIL_URL1 =
            "https://encrypted-tbn3.gstatic.com/images?q=tbn:ANd9GcRydPYA9MW5_G1lKk6fM8OVNf9z7lF5e7ZI2hAAVIAAb-_b3eyQrCQN6j2AcNAaWu-KO11XpQfC-A";
    private static final String TITLE1 =
            "Jerry Jones: Owners should approve of Roger Goodell's decisions (2097)";
    // The second suggestion URL and thumbnail URL.
    private static final String URL2 =
            "https://www.nytimes.com/2017/11/10/world/asia/trump-apec-asia-trade.html";
    private static final String THUMBNAIL_URL2 =
            "https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRh1tEaJT-br6mBxM89U3vgjDldwb9L_baZszhstAGMQh3_fuG13ax3C9ewR2tq45tbZj74CHl3KNU";

    // Returns a small PNG image data.
    private static byte[] testImageData() {
        final String imageBase64 =
                "iVBORw0KGgoAAAANSUhEUgAAAAQAAAAECAYAAACp8Z5+AAAABHNCSVQICAgIfAhkiAAAADRJREFU"
                + "CJlNwTERgDAABLA8h7AaqCwUdcYCmOFYn5UkbSsBWvsU7/GAM7H5u4a07RTrHuADaewQm6Wdp7oA"
                + "AAAASUVORK5CYII=";
        return Base64.decode(imageBase64, Base64.DEFAULT);
    }
    private static final int THUMBNAIL_WIDTH = 4;
    private static final int THUMBNAIL_HEIGHT = 4;

    // A fake NetworkChangeNotifierAutoDetect which always reports a connection.
    private static class FakeAutoDetect extends NetworkChangeNotifierAutoDetect {
        public FakeAutoDetect(Observer observer, RegistrationPolicy policy) {
            super(observer, policy);
        }
        @Override
        public NetworkState getCurrentNetworkState() {
            return new NetworkState(true, ConnectivityManager.TYPE_WIFI, 0, null, false);
        }
    }

    // Helper for checking isPrefetchingEnabledByServer().
    private boolean isEnabledByServer() {
        final AtomicBoolean isEnabled = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isEnabled.set(PrefetchConfiguration.isPrefetchingEnabledByServer()); });
        return isEnabled.get();
    }

    private void waitForServerEnabledValue(boolean wanted) {
        CriteriaHelper.pollUiThread(() -> {
            return PrefetchConfiguration.isPrefetchingEnabledByServer() == wanted;
        }, "never got wanted value", 5000, 200);
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        TestNetworkClient client = new TestNetworkClient();
        client.setNetworkResponseFile(TEST_FEED);
        FeedProcessScopeFactory.setTestNetworkClient(client);

        // Start the server before Chrome starts, offline_pages_backend is overridden later with
        // this server's address.
        mServer = new WebServer(0, false);
        mServer.setRequestHandler(this);
        // Inject FakeAutoDetect so that the download component will attempt to download a file even
        // when there is no connection.
        NetworkStatusListenerAndroid.setAutoDetectFactory(
                new NetworkStatusListenerAndroid.AutoDetectFactory() {
                    @Override
                    public NetworkChangeNotifierAutoDetect create(
                            NetworkChangeNotifierAutoDetect.Observer observer,
                            NetworkChangeNotifierAutoDetect.RegistrationPolicy policy) {
                        return new FakeAutoDetect(observer, policy);
                    }
                });

        // Configure flags:
        // - Enable OfflinePagesPrefetching and InterestFeedContentSuggestions.
        // - Set DownloadService's start_up_delay_ms to 100 ms (5 seconds is default).
        // - Set offline_pages_backend.
        final String offlinePagesBackend = Uri.encode(mServer.getBaseUrl());
        CommandLine.getInstance().appendSwitchWithValue("enable-features",
                ChromeFeatureList.OFFLINE_PAGES_PREFETCHING + "<Trial,DownloadService<Trial"
                        + "," + ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS + "<Trial");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrials", "Trial/Group");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrial-params",
                "Trial.Group:start_up_delay_ms/100/offline_pages_backend/" + offlinePagesBackend);

        // TestOfflinePageService will send GCM. Enable fake GCM.
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(true);

        // Start Chrome.
        mActivityTestRule.startMainActivityOnBlankPage();

        // Register Offline Page observer and enable limitless prefetching.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfile = mActivityTestRule.getActivity().getActivityTab().getProfile();
            OfflinePageBridge.getForProfile(mProfile).addObserver(
                    new OfflinePageBridge.OfflinePageModelObserver() {
                        @Override
                        public void offlinePageAdded(OfflinePageItem addedPage) {
                            mAddedPages.add(addedPage);
                            mPageAddedHelper.notifyCalled();
                        }
                    });
            PrefetchTestBridge.enableLimitlessPrefetching(true);
            PrefetchTestBridge.skipNTPSuggestionsAPIKeyCheck();
        });

        OfflineTestUtil.setPrefetchingEnabledByServer(true);
        OfflineTestUtil.setGCMTokenForTesting("dummy_gcm_token");
    }

    @After
    public void tearDown() throws Exception {
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(false);
        mServer.shutdown();
    }

    @Override
    public void handleRequest(WebServer.HTTPRequest request, OutputStream stream) {
        try {
            if (mOPS.handleRequest(request, stream)) {
                // Handled.
            } else {
                Assert.fail("Unhandled request: " + request.toString());
            }
        } catch (IOException e) {
            Assert.fail(e.getMessage() + " \n while handling request: " + request.toString());
        }
    }

    private static OfflineContentProvider offlineContentProvider() {
        return OfflineContentAggregatorFactory.forProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
    }

    private OfflineItem findItemByUrl(String url) throws InterruptedException, TimeoutException {
        for (OfflineItem item : OfflineTestUtil.getOfflineItems()) {
            if (item.pageUrl.equals(url)) {
                return item;
            }
        }
        return null;
    }

    private Bitmap findVisuals(ContentId id) throws InterruptedException, TimeoutException {
        final CallbackHelper finished = new CallbackHelper();
        final AtomicReference<Bitmap> result = new AtomicReference<Bitmap>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            offlineContentProvider().getVisualsForItem(id, (resultId, visuals) -> {
                if (visuals != null) {
                    result.set(visuals.icon);
                }
                finished.notifyCalled();
            });
        });
        finished.waitForCallback(0);
        return result.get();
    }

    private void runAndWaitForBackgroundTask() throws Throwable {
        final CallbackHelper finished = new CallbackHelper();
        PrefetchBackgroundTask task = new PrefetchBackgroundTask();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TaskParameters.Builder builder =
                    TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
            PrefetchBackgroundTask.skipConditionCheckingForTesting();
            task.onStartTask(ContextUtils.getApplicationContext(), builder.build(),
                    (boolean needsReschedule) -> { finished.notifyCalled(); });
        });
        finished.waitForCallback(0);
    }

    /**
     * Waits for |callbackHelper| to be notified. Runs the background task while waiting.
     * @param callbackHelper The callback helper to wait for.
     * @param currentCallCount |callbackHelper|'s current call count.
     * @param numberOfCallsToWaitFor number of calls to wait for.
     */
    private void runBackgroundTaskUntilCallCountReached(CallbackHelper callbackHelper,
            int currentCallCount, int numberOfCallsToWaitFor) throws Throwable {
        // It's necessary to run the background task multiple times because we don't always have a
        // hook to know when the system is ready for background processing.
        long startTime = System.nanoTime();
        final long timeoutTime = startTime + TimeUnit.SECONDS.toNanos(5);
        while (true) {
            runAndWaitForBackgroundTask();
            try {
                callbackHelper.waitForCallback(
                        currentCallCount, numberOfCallsToWaitFor, 200, TimeUnit.MILLISECONDS);
                return;
            } catch (TimeoutException e) {
                if (System.nanoTime() > timeoutTime) {
                    throw e;
                }
            }
        }
    }

    /** Trigger conditions required to load NTP snippets. */
    private void forceLoadSnippets() throws Throwable {
        // NTP suggestions require a connection and an accepted EULA.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NetworkChangeNotifier.forceConnectivityState(true);
            PrefServiceBridge.getInstance().setEulaAccepted();
        });

        // Loading the NTP triggers loading suggestions.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
    }

    /**
     * Serve a single suggestion to NTP snippets. That suggestion is successfully handled by
     * offline prefetch.
     */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetchFeed"})
    public void testPrefetchSinglePageSuccess() throws Throwable {
        // TODO(crbug.com/845310): Expand this test. There's some important flows missing and
        // systems missing.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefetchTestBridge.insertIntoCachedImageFetcher(THUMBNAIL_URL1, testImageData());
        });

        // Set up default (success) OPS behavior for a URL.
        TestOfflinePageService.PageBehavior behavior = new TestOfflinePageService.PageBehavior();
        mOPS.setPageBehavior(URL1, behavior);
        // Set up failure status for all other URLs.
        mOPS.setDefaultGenerateStatus(StatusOuterClass.Code.UNKNOWN);

        forceLoadSnippets();

        // Wait for the page to be added to the offline model.
        runBackgroundTaskUntilCallCountReached(mPageAddedHelper, 0, 1);

        // Check that the page was downloaded and saved.
        Assert.assertEquals(1, mOPS.ReadCalled.getCallCount());
        Assert.assertTrue(mAddedPages.size() >= 1);
        Assert.assertEquals(URL1, mAddedPages.get(0).getUrl());
        Assert.assertEquals(TITLE1, mAddedPages.get(0).getTitle());
        Assert.assertEquals(behavior.body.length, mAddedPages.get(0).getFileSize());
        Assert.assertNotEquals("", mAddedPages.get(0).getFilePath());

        // Check that the thumbnail was fetched.
        OfflineItem item1 = findItemByUrl(URL1);
        Assert.assertTrue(item1 != null);
        Bitmap visuals = findVisuals(item1.id);
        Assert.assertTrue(visuals != null);
        Assert.assertEquals(THUMBNAIL_WIDTH, visuals.getWidth());
        Assert.assertEquals(THUMBNAIL_HEIGHT, visuals.getHeight());
    }

    /**
     *  Request two pages. One is ready later, and one fails immediately.
     *
     *  WARNING: this test might be flakey, sometimes waiting for the callback times out regardless.
     */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetchFeed"})
    public void testPrefetchPageReadyLater() throws Throwable {
        TestOfflinePageService.PageBehavior pageFail = new TestOfflinePageService.PageBehavior();
        pageFail.generateStatus = StatusOuterClass.Code.UNKNOWN;
        pageFail.getStatus = StatusOuterClass.Code.UNKNOWN;
        mOPS.setPageBehavior(URL1, pageFail);

        TestOfflinePageService.PageBehavior readyLater = new TestOfflinePageService.PageBehavior();
        readyLater.generateStatus = StatusOuterClass.Code.NOT_FOUND;
        mOPS.setPageBehavior(URL2, readyLater);

        // Set up failure status for all other URLs.
        mOPS.setDefaultGenerateStatus(StatusOuterClass.Code.UNKNOWN);

        forceLoadSnippets();
        runBackgroundTaskUntilCallCountReached(mOPS.GeneratePageBundleCalled, 0, 1);

        // At this point, the bundle has been requested. Send the GCM message, and wait for the page
        // to be imported. If this assert fails, GeneratePageBundle may not have been called.
        Assert.assertEquals("operation-1", mOPS.sendPushMessage());
        runAndWaitForBackgroundTask();
        mPageAddedHelper.waitForCallback(0, 1);

        // Only URL2 is added.
        Assert.assertEquals(1, mAddedPages.size());
        Assert.assertEquals(URL2, mAddedPages.get(0).getUrl());
        Assert.assertEquals(readyLater.body.length, mAddedPages.get(0).getFileSize());
        Assert.assertNotEquals("", mAddedPages.get(0).getFilePath());
    }

    /** Request a page and get a Forbidden response. The enabled-by-server state should change. */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetchFeed"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/950749
    public void testPrefetchForbiddenByServer() throws Throwable {
        mOPS.setForbidGeneratePageBundle(true);

        Assert.assertTrue(isEnabledByServer());

        forceLoadSnippets();
        runBackgroundTaskUntilCallCountReached(mOPS.GeneratePageBundleCalled, 0, 1);
        waitForServerEnabledValue(false);

        Assert.assertFalse(isEnabledByServer());
    }

    /**
     * Check that a server-enabled check can enable prefetching.
     */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetchFeed"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/950749
    public void testPrefetchBecomesEnabledByServer() throws Throwable {
        OfflineTestUtil.setPrefetchingEnabledByServer(false);

        Assert.assertFalse(isEnabledByServer());

        forceLoadSnippets();
        waitForServerEnabledValue(true);

        Assert.assertTrue(isEnabledByServer());
    }

    /**
     * Check that prefetching remains disabled by the server after receiving a forbidden
     * response.
     */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetchFeed"})
    public void testPrefetchRemainsDisabledByServer() throws Throwable {
        OfflineTestUtil.setPrefetchingEnabledByServer(false);
        mOPS.setForbidGeneratePageBundle(true);

        Assert.assertFalse(isEnabledByServer());

        forceLoadSnippets();
        waitForServerEnabledValue(false);

        Assert.assertFalse(isEnabledByServer());
    }
}

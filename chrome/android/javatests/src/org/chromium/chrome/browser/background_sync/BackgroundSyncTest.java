// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.after;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.background_task_scheduler.ChromeBackgroundTaskFactory;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.BackgroundSyncNetworkUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation test for Background Sync.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class BackgroundSyncTest {
    @Mock
    private BackgroundTaskScheduler mTaskScheduler;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private static final String BACKGROUND_SYNC_TEST_PAGE =
            "/chrome/test/data/background_sync/background_sync_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(10);
    private static final long WAIT_TIME_MS = scaleTimeout(5000);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);

        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        ChromeBackgroundTaskFactory.setAsDefault();
        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(ContextUtils.getApplicationContext()), any(TaskInfo.class));

        // loadNativeLibraryAndInitBrowserProcess will access AccountManagerFacade, so it should
        // be initialized beforehand.
        SigninTestUtil.setUpAuthForTest();

        // This is necessary because our test devices don't have Google Play Services up to date,
        // and BackgroundSync requires that. Remove this once https://crbug.com/514449 has been
        // fixed.
        // Note that this should be done before the startMainActivityOnBlankPage(), because Chrome
        // will otherwise run this check on startup and disable BackgroundSync code.
        if (!ExternalAuthUtils.canUseGooglePlayServices()) {
            mNativeLibraryTestRule.loadNativeLibraryAndInitBrowserProcess();
            disableGooglePlayServicesVersionCheck();
        }

        mActivityTestRule.startMainActivityOnBlankPage();

        // BackgroundSync only works with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
        SigninTestUtil.tearDownAuthForTest();
    }

    @Test
    @MediumTest
    @Feature({"BackgroundSync"})
    public void onSyncCalledWithNetworkConnectivity() throws Exception {
        forceConnectionType(ConnectionType.CONNECTION_NONE);

        mActivityTestRule.loadUrl(mTestServer.getURL(BACKGROUND_SYNC_TEST_PAGE));
        runJavaScript("SetupReplyForwardingForTests();");

        // Register Sync.
        runJavaScript("RegisterSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("registered sync");
        verify(mTaskScheduler, timeout(WAIT_TIME_MS))
                .schedule(eq(ContextUtils.getApplicationContext()),
                        argThat(taskInfo
                                -> taskInfo.getBackgroundTaskClass()
                                        == BackgroundSyncBackgroundTask.class));

        forceConnectionType(ConnectionType.CONNECTION_WIFI);
        assertTitleBecomes("onsync: tagSucceedsSync");

        // Another invocation when all events are firing but haven't completed, to cover the case
        // when the browser is closed mid-event. This one races with the completion of the sync
        // event, and might not happen.
        // The wait is to ensure we wait for this invocation to happen, since it happens in
        // parallel with dispatching the sync event.
        verify(mTaskScheduler, after(WAIT_TIME_MS).atMost(2))
                .schedule(eq(ContextUtils.getApplicationContext()),
                        argThat(taskInfo
                                -> taskInfo.getBackgroundTaskClass()
                                        == BackgroundSyncBackgroundTask.class));
    }

    @Test
    @MediumTest
    @Feature({"BackgroundSync"})
    public void browserWakeUpScheduledWhenSyncEventFails() throws Exception {
        forceConnectionType(ConnectionType.CONNECTION_NONE);

        mActivityTestRule.loadUrl(mTestServer.getURL(BACKGROUND_SYNC_TEST_PAGE));
        runJavaScript("SetupReplyForwardingForTests();");

        // Register Sync.
        runJavaScript("RegisterSyncForTag('tagFailsSync');");
        assertTitleBecomes("registered sync");
        verify(mTaskScheduler, timeout(WAIT_TIME_MS))
                .schedule(eq(ContextUtils.getApplicationContext()),
                        argThat(taskInfo
                                -> taskInfo.getBackgroundTaskClass()
                                        == BackgroundSyncBackgroundTask.class));

        forceConnectionType(ConnectionType.CONNECTION_WIFI);

        // Wait for some time that is less than the retry time period (default 5 minutes).
        // One call to schedule wake-up a few minutes after firing the first sync event, to cover
        // our bases if the browser is closed mid-sync. This one races with the completion of the
        // sync event, and might not happen.
        // Another call to waking up the browser for attempt two after completion of the first
        // event. This will always happen.
        verify(mTaskScheduler, after(WAIT_TIME_MS).atMost(3))
                .schedule(eq(ContextUtils.getApplicationContext()),
                        argThat(taskInfo
                                -> taskInfo.getBackgroundTaskClass()
                                        == BackgroundSyncBackgroundTask.class));
    }

    /**
     * Helper methods.
     */
    private String runJavaScript(String code) throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    @SuppressWarnings("MissingFail")
    private void assertTitleBecomes(String expectedTitle) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, tab.getTitle());
        }
    }

    private void forceConnectionType(int connectionType) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { BackgroundSyncNetworkUtils.setConnectionTypeForTesting(connectionType); });
    }

    private void disableGooglePlayServicesVersionCheck() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BackgroundSyncBackgroundTaskSchedulerJni.get()
                    .setPlayServicesVersionCheckDisabledForTests(
                            /* disabled= */ true);
        });
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.ExecutionException;

/**
 * Integration tests for {@link PageViewObserver} and {@link SuspendedTab}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // Direct all hostnames to EmbeddedTestServer running on 127.0.0.1.
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public class TabSuspensionTest {
    private static final String STARTING_FQDN = "example.com";
    private static final String DIFFERENT_FQDN = "www.google.com";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Mock
    private UsageStatsBridge mUsageStatsBridge;
    @Mock
    private SuspensionTracker mSuspensionTracker;

    private ChromeTabbedActivity mActivity;
    private PageViewObserver mPageViewObserver;
    private TokenTracker mTokenTracker;
    private EventTracker mEventTracker;
    private Tab mTab;
    private EmbeddedTestServer mTestServer;
    private String mStartingUrl;
    private String mDifferentUrl;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        // TokenTracker and EventTracker hold a promise, and Promises can only be used on a single
        // thread, so we have to initialize them on the thread where they will be used.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTokenTracker = new TokenTracker(mUsageStatsBridge);
            mEventTracker = new EventTracker(mUsageStatsBridge);
        });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mStartingUrl = mTestServer.getURLWithHostName(STARTING_FQDN, "/defaultresponse");
        mDifferentUrl = mTestServer.getURLWithHostName(DIFFERENT_FQDN, "/defaultresponse");

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        mPageViewObserver = new PageViewObserver(mActivity, mActivity.getTabModelSelector(),
                mEventTracker, mTokenTracker, mSuspensionTracker);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void testNavigateToSuspended() throws InterruptedException {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        startLoadingUrl(mTab, mDifferentUrl);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mDifferentUrl);
        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    public void testNavigateToSuspendedDomain_differentPage() throws InterruptedException {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        startLoadingUrl(mTab, mStartingUrl + "foo.html");
        assertSuspendedTabShowing(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testNewTabSuspended() throws InterruptedException {
        mActivityTestRule.loadUrl(mStartingUrl);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        // We can't use loadUrlInNewTab because the site being suspended will prevent loading from
        // completing, and loadUrlInNewTab expects loading to succeed.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab2 = mActivity.getActivityTab();

        startLoadingUrl(tab2, mDifferentUrl);
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);
    }

    @Test
    @MediumTest
    public void testTabSwitchBackToSuspended() throws InterruptedException {
        mActivityTestRule.loadUrl(mStartingUrl);
        final int originalTabIndex =
                mActivity.getTabModelSelector().getCurrentModel().indexOf(mTab);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mDifferentUrl);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    originalTabIndex, TabSelectionType.FROM_USER);
        });
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testEagerSuspension() throws InterruptedException {
        mActivityTestRule.loadUrl(mStartingUrl);
        suspendDomain(STARTING_FQDN);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        // Suspending again shouldn't crash or otherwise affect the state of the world.
        suspendDomain(STARTING_FQDN);
        assertSuspendedTabShowing(mTab, STARTING_FQDN);

        // A single un-suspend should be sufficient even though we triggered suspension twice.
        unsuspendDomain(STARTING_FQDN);
        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    public void testMultiWindow() throws InterruptedException {
        mActivityTestRule.loadUrl(mStartingUrl);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mDifferentUrl);
        suspendDomain(DIFFERENT_FQDN);
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity2 activity2 =
                MultiWindowTestHelper.waitForSecondChromeTabbedActivity();
        MultiWindowTestHelper.waitForTabs("CTA", activity2, 1, tab2.getId());
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);

        // Each PageViewObserver is associated with a single ChromeTabbedActivity, so we need to
        // create a new one for the other window.
        PageViewObserver pageViewObserver2 = new PageViewObserver(activity2,
                activity2.getTabModelSelector(), mEventTracker, mTokenTracker, mSuspensionTracker);

        suspendDomain(STARTING_FQDN);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { pageViewObserver2.notifySiteSuspensionChanged(DIFFERENT_FQDN, false); });
        // Suspending and un-suspending should work in both activities/windows.
        assertSuspendedTabHidden(tab2);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testTabAddedFromCustomTab() throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), mStartingUrl));
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mCustomTabActivityTestRule.getActivity(), R.id.open_in_browser_id);
        MultiWindowTestHelper.waitForTabs("CustomTab", mActivity, 2, Tab.INVALID_TAB_ID);
        waitForSuspendedTabToShow(mActivity.getActivityTab(), STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testTabAddedInBackground() throws InterruptedException, ExecutionException {
        Tab bgTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return mActivity.getCurrentTabCreator().createNewTab(
                    new LoadUrlParams(mStartingUrl), TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab);
        });
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mStartingUrl);

        suspendDomain(STARTING_FQDN);
        assertSuspendedTabHidden(bgTab);
    }

    @Test
    @MediumTest
    public void testTabUnsuspendedInBackground() throws InterruptedException {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        mActivityTestRule.loadUrl(mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
        final int originalTabIndex =
                mActivity.getTabModelSelector().getCurrentModel().indexOf(mTab);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mDifferentUrl);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPageViewObserver.notifySiteSuspensionChanged(STARTING_FQDN, false);
            doReturn(false).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    originalTabIndex, TabSelectionType.FROM_USER);
        });

        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    public void testNavigationFromSuspendedTabToInterstitial() throws InterruptedException {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        mActivityTestRule.loadUrl(mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        MockSafeBrowsingApiHandler.addMockResponse(
                mDifferentUrl, "{\"matches\":[{\"threat_type\":\"5\"}]}");
        mActivityTestRule.loadUrl(mDifferentUrl);

        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    public void testRendererCrashOnSuspendedTab() throws InterruptedException {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        mActivityTestRule.loadUrl(mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabTestUtils.simulateCrash(mTab, true);
            assertSuspendedTabHidden(mTab);
        });
    }

    private void startLoadingUrl(Tab tab, String url) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.loadUrl(new LoadUrlParams(url, PageTransition.TYPED)); });
    }

    private void assertSuspendedTabHidden(Tab tab) {
        assertSuspendedTabState(tab, false, null);
    }

    private void assertSuspendedTabShowing(Tab tab, String fqdn) {
        assertSuspendedTabState(tab, true, fqdn);
    }

    private void assertSuspendedTabState(Tab tab, boolean showing, String fqdn) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SuspendedTab suspendedTab = SuspendedTab.from(tab);
            assertEquals(suspendedTab.isShowing(), showing);
            assertEquals(suspendedTab.isViewAttached(), showing);
            assertTrue((suspendedTab.getFqdn() == null && fqdn == null)
                    || fqdn.equals(suspendedTab.getFqdn()));
        });
    }

    private void suspendDomain(String domain) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPageViewObserver.notifySiteSuspensionChanged(domain, true); });
    }

    private void unsuspendDomain(String domain) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPageViewObserver.notifySiteSuspensionChanged(domain, false); });
    }

    private void waitForSuspendedTabToShow(Tab tab, String fqdn) throws InterruptedException {
        CriteriaHelper.pollUiThread(() -> {
            return SuspendedTab.from(tab).isShowing();
        }, "Suspended tab should be showing", 10000, 50);

        assertSuspendedTabShowing(tab, fqdn);
    }
}
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.chrome.browser.tabmodel.TabSelectionType.FROM_USER;
import static org.chromium.chrome.browser.util.UrlConstants.NTP_URL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for the {@link GridTabSwitcherLayout} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class GridTabSwitcherLayoutTest {
    private static final String TAG = "GTSLayoutTest";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private GridTabSwitcherLayout mGtsLayout;
    private String mUrl;
    private int mRepeat;

    @Before
    public void setUp() throws InterruptedException {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityFromLauncher();

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof GridTabSwitcherLayout);
        mGtsLayout = (GridTabSwitcherLayout) layout;
        mUrl = testServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 3;
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/0/cleanup-delay/0"})
    public void testTabToGridFromLiveTab() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.
            Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/0/cleanup-delay/0"})
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/0/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.
            Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/0/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:soft-cleanup-delay/0/cleanup-delay/0"})
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(NTP_URL);
    }

    /**
     * Make Chrome have {@code numTabs} or Tabs with {@code url} loaded.
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *            be saved.
     */
    private void prepareTabs(int numTabs, @Nullable String url) throws InterruptedException {
        assertTrue(numTabs >= 1);
        assertEquals(1, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (url != null) mActivityTestRule.loadUrl(url);
        for (int i = 0; i < numTabs - 1; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
            if (url != null) mActivityTestRule.loadUrl(url);
        }
        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(), null,
                null, WAIT_TIMEOUT_SECONDS * 10);
        assertEquals(
                numTabs, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    private void testTabToGrid(String fromUrl) throws InterruptedException {
        mActivityTestRule.loadUrl(fromUrl);

        int initCount = getCaptureCount();

        GridTabSwitcher gts = mGtsLayout.getGridTabSwitcherForTesting();
        for (int i = 0; i < mRepeat; i++) {
            int count = getCaptureCount();
            waitForCaptureRateControl();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(true));
            assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

            // Make sure the fading animation is done.
            int delta;
            if (TextUtils.equals(mActivityTestRule.getActivity()
                                         .getCurrentWebContents()
                                         .getLastCommittedUrl(),
                        NTP_URL)) {
                delta = 0;
            } else {
                delta = 1;
                // TODO(wychen): refactor areAnimatorsEnabled() to a util class.
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                        && TabGridContainerViewBinderTest.areAnimatorsEnabled()) {
                    delta += 1;
                }
            }
            CriteriaHelper.pollUiThread(Criteria.equals(delta, () -> getCaptureCount() - count));

            // clang-format off
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> gts.getGridController().hideOverview(false));
            CriteriaHelper.pollInstrumentationThread(
                    () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                    "Overview not hidden yet", DEFAULT_MAX_TIME_TO_POLL * 10,
                    DEFAULT_POLLING_INTERVAL);
            // clang-format on
        }
        int expected;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            expected = 0;
        } else {
            expected = mRepeat;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && TabGridContainerViewBinderTest.areAnimatorsEnabled()) {
                expected += mRepeat;
            }
        }
        Assert.assertEquals(expected, getCaptureCount() - initCount);
    }

    @Test
    @MediumTest
    public void testGridToTabToCurrentNTP() throws InterruptedException {
        prepareTabs(1, NTP_URL);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToOtherNTP() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToCurrentLive() throws InterruptedException {
        prepareTabs(1, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToCurrentLiveWithAnimation() throws InterruptedException {
        prepareTabs(1, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToOtherLive() throws InterruptedException {
        prepareTabs(2, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToOtherLiveWithAnimation() throws InterruptedException {
        prepareTabs(2, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        prepareTabs(2, mUrl);
        testGridToTab(true, true);
    }

    private void testGridToTab(boolean switchToAnotherTab, boolean killBeforeSwitching)
            throws InterruptedException {
        final int initCount = getCaptureCount();

        for (int i = 0; i < mRepeat; i++) {
            int count = getCaptureCount();
            waitForCaptureRateControl();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(true));
            assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
            int delta;
            if (TextUtils.equals(mActivityTestRule.getActivity()
                                         .getCurrentWebContents()
                                         .getLastCommittedUrl(),
                        NTP_URL)) {
                delta = 0;
            } else {
                delta = 1;
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                        && TabGridContainerViewBinderTest.areAnimatorsEnabled()) {
                    delta += 1;
                }
            }
            CriteriaHelper.pollUiThread(Criteria.equals(delta, () -> getCaptureCount() - count));

            int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
            final int targetIndex = switchToAnotherTab ? 1 - index : index;
            Tab targetTab =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(targetIndex);
            if (killBeforeSwitching) {
                WebContentsUtils.simulateRendererKilled(targetTab.getWebContents(), false);
            }

            if (switchToAnotherTab) {
                waitForCaptureRateControl();
            }
            int count2 = getCaptureCount();
            // clang-format off
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getCurrentTabModel().setIndex(
                            targetIndex, FROM_USER));
            CriteriaHelper.pollInstrumentationThread(
                    () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                    "Overview not hidden yet");
            // clang-format on
            if (switchToAnotherTab
                    && !TextUtils.equals(mActivityTestRule.getActivity()
                                                 .getCurrentWebContents()
                                                 .getLastCommittedUrl(),
                            NTP_URL)) {
                delta = 1;
            } else {
                delta = 0;
            }
            CriteriaHelper.pollUiThread(Criteria.equals(delta, () -> getCaptureCount() - count2));
        }
        int expected;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            expected = 0;
        } else {
            expected = mRepeat;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && TabGridContainerViewBinderTest.areAnimatorsEnabled()) {
                expected += mRepeat;
            }
            if (switchToAnotherTab) {
                expected += mRepeat;
            }
        }
        Assert.assertEquals(expected, getCaptureCount() - initCount);
    }

    private int getCaptureCount() {
        return RecordHistogram.getHistogramTotalCountForTesting("Compositing.CopyFromSurfaceTime");
    }

    private void waitForCaptureRateControl() throws InterruptedException {
        // Needs to wait for |kCaptureMinRequestTimeMs| in order to capture another one.
        // TODO(wychen): mock |kCaptureMinRequestTimeMs| to 0 in tests?
        Thread.sleep(2000);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests the functionality of return to chrome features that open overview mode if the timeout
 * has passed.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
        "force-fieldtrials=Study/Group"})
public class ReturnToChromeTest {
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:" + TAB_SWITCHER_ON_RETURN_MS + "/0";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private String mUrl;

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Before
    public void setUp() {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUrl = testServer.getURL("/chrome/test/data/android/about.html");

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Test that overview mode is not triggered if the delay is longer than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/100000"})
    public void testTabSwitcherModeNotTriggeredWithinThreshold() throws Exception {
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 2, 0, mUrl);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT));
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    @FlakyTest(message = "crbug.com/1040895")
    public void testTabSwitcherModeTriggeredBeyondThreshold() throws Exception {
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 2, 0, mUrl);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT));

        mActivityTestRule.startMainActivityFromLauncher();

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (!mActivityTestRule.getActivity().isTablet()) {
            CriteriaHelper.pollUiThread(Criteria.equals(1,
                    ()
                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                    ReturnToChromeExperimentsUtil
                                            .UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(true)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(true)
                            + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                    mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTotalTabCount())));
        }
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA.
     */
    @Test
    @MediumTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    @FlakyTest(message = "crbug.com/1040896")
    public void testTabSwitcherModeTriggeredBeyondThreshold_WarmStart() throws Exception {
        testTabSwitcherModeTriggeredBeyondThreshold();

        // Redo to trigger warm startup UMA.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (!mActivityTestRule.getActivity().isTablet()) {
            CriteriaHelper.pollUiThread(Criteria.equals(2,
                    ()
                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                    ReturnToChromeExperimentsUtil
                                            .UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(false)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(false)
                            + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                    mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTotalTabCount())));
        }
    }

    /**
     * Similar to {@link ChromeTabbedActivityTestRule#startMainActivityFromLauncher} but skip
     * verification and tasks regarding current tab.
     */
    private void startMainActivityFromLauncherWithoutCurrentTab() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.startActivityCompletely(intent);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start. Also test the first meaningful paint UMA for the no-tab condition.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    @DisabledTest(message = "http://crbug.com/1027315")
    public void testTabSwitcherModeTriggeredBeyondThreshold_NoTabs() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().closeAllTabs());
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT));

        // Cannot use ChromeTabbedActivityTestRule.startMainActivityFromLauncher() because
        // there's no tab.
        startMainActivityFromLauncherWithoutCurrentTab();

        if (!mActivityTestRule.getActivity().isTablet()) {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(0, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (!mActivityTestRule.getActivity().isTablet()) {
            CriteriaHelper.pollUiThread(Criteria.equals(1,
                    ()
                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                    ReturnToChromeExperimentsUtil
                                            .UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(true)));
            assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            ReturnToChromeExperimentsUtil.UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                            + ReturnToChromeExperimentsUtil.coldStartBucketName(true)
                            + ReturnToChromeExperimentsUtil.numThumbnailsBucketName(
                                    mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTotalTabCount())));
        }
    }

    @Test
    @SmallTest
    @Feature({"ReturnToChrome", "RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(hardware_is = "bullhead", message = "https://crbug.com/1025241")
    public void testInitialScrollIndex() throws Exception {
        // clang-format on
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 10, 0, mUrl);

        // Trigger thumbnail capturing for the last tab.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());

        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(10, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        assertEquals(9, mActivityTestRule.getActivity().getCurrentTabModel().index());
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(
                                       org.chromium.chrome.tab_ui.R.id.tab_list_view),
                "10_web_tabs-select_last");
    }
}

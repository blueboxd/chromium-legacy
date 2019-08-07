// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.chrome.browser.util.UrlConstants.NTP_URL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.graphics.Bitmap;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.ViewActions;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.LinkedList;
import java.util.List;

/** Tests for the {@link StartSurfaceLayout} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class StartSurfaceLayoutTest {
    private static final String TAG = "SSLayoutTest";
    private static final String BASE_PARAMS = "force-fieldtrial-params="
            + "Study.Group:soft-cleanup-delay/0/cleanup-delay/0/skip-slow-zooming/false"
            + "/zooming-min-sdk-version/19/zooming-min-memory-mb/512";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private StartSurfaceLayout mStartSurfaceLayout;
    private String mUrl;
    private int mRepeat;
    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private Callback<Bitmap> mBitmapListener =
            (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));

    @Before
    public void setUp() throws InterruptedException {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityFromLauncher();

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        mUrl = testServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 3;

        TabSwitcher.TabListDelegate delegate =
                mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        delegate.setBitmapCallbackForTesting(mBitmapListener);
        Assert.assertEquals(0, delegate.getBitmapFetchCountForTesting());

        mActivityTestRule.getActivity().getTabContentManager().setCaptureMinRequestTimeForTesting(
                0);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromLiveTab() throws InterruptedException {
        TabSwitcher.TabListDelegate delegate =
                mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        assertEquals(0, delegate.getSoftCleanupDelayForTesting());
        assertEquals(0, delegate.getCleanupDelayForTesting());

        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
        assertThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        // clang-format on
        assertTrue(FeatureUtilities.isTabToGtsAnimationEnabled());

        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
        assertThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        TabSwitcher.TabListDelegate delegate =
                mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        assertEquals(10000, delegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, delegate.getCleanupDelayForTesting());

        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(2, NTP_URL);
        testTabToGrid(NTP_URL);
        assertThumbnailsAreReleased();
    }

    /**
     * Make Chrome have {@code numTabs} or Tabs with {@code url} loaded.
     * @see #prepareTabs(int, String, boolean)
     */
    private void prepareTabs(int numTabs, @Nullable String url) throws InterruptedException {
        prepareTabs(numTabs, url, true);
    }

    /**
     * Make Chrome have {@code numTabs} or Tabs with {@code url} loaded.
     * @param numTabs The number of tabs we expect after finishing
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *            be saved.
     * @param waitForLoading Whether wait for URL loading.
     */
    private void prepareTabs(int numTabs, @Nullable String url, boolean waitForLoading)
            throws InterruptedException {
        assertTrue(numTabs >= 1);
        assertEquals(1, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (url != null) mActivityTestRule.loadUrl(url);
        for (int i = 0; i < numTabs - 1; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
            if (url != null) mActivityTestRule.loadUrl(url);
            if (!waitForLoading) continue;

            Tab previousTab = mActivityTestRule.getActivity()
                                      .getTabModelSelector()
                                      .getCurrentModel()
                                      .getTabAt(i);

            boolean fixPendingReadbacks = mActivityTestRule.getActivity()
                                                  .getTabContentManager()
                                                  .getPendingReadbacksForTesting()
                    != 0;

            // When there are pending readbacks due to detached Tabs, try to fix it by switching
            // back to that tab.
            if (fixPendingReadbacks) {
                int lastIndex = i;
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() ->
                    mActivityTestRule.getActivity().getCurrentTabModel().setIndex(
                            lastIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }
            checkThumbnailsExist(previousTab);
            if (fixPendingReadbacks) {
                int currentIndex = i + 1;
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() ->
                    mActivityTestRule.getActivity().getCurrentTabModel().setIndex(
                            currentIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }
        }
        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(), null,
                null, WAIT_TIMEOUT_SECONDS * 10);
        assertEquals(
                numTabs, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());

        if (waitForLoading) {
            // clang-format off
            CriteriaHelper.pollUiThread(Criteria.equals(0, () ->
                mActivityTestRule.getActivity()
                        .getTabContentManager()
                        .getPendingReadbacksForTesting()
            ));
            // clang-format on
        }
    }

    private void testTabToGrid(String fromUrl) throws InterruptedException {
        mActivityTestRule.loadUrl(fromUrl);

        final int initCount = getCaptureCount();

        StartSurface startSurface = mStartSurfaceLayout.getStartSurfaceForTesting();
        for (int i = 0; i < mRepeat; i++) {
            enterGTS();

            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { startSurface.getController().onBackPressed(); });
            // clang-format off
            CriteriaHelper.pollInstrumentationThread(
                    () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                    "Overview not hidden yet", DEFAULT_MAX_TIME_TO_POLL * 10,
                    DEFAULT_POLLING_INTERVAL);
            // clang-format on
        }
        checkFinalCaptureCount(false, initCount);
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
    @DisabledTest(message = "crbug.com/986047. This works on emulators but not on real devices.")
    public void testGridToTabToCurrentLiveDetached() throws Exception {
        for (int i = 0; i < 10; i++) {
            // Quickly create some tabs, navigate to web pages, and don't wait for thumbnail
            // capturing.
            prepareTabs(2, mUrl, false);
            // Hopefully we are in a state where some pending readbacks are stuck because their tab
            // is not attached to the view.
            if (mActivityTestRule.getActivity()
                            .getTabContentManager()
                            .getPendingReadbacksForTesting()
                    > 0)
                break;

            // Restart Chrome.
            // Although we're destroying the activity, the Application will still live on since its
            // in the same process as this test.
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getTabModelSelector().closeAllTabs());
            ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
            mActivityTestRule.startMainActivityOnBlankPage();
            Assert.assertEquals(1, mActivityTestRule.tabsCount(false));
        }
        assertNotEquals(0,
                mActivityTestRule.getActivity()
                        .getTabContentManager()
                        .getPendingReadbacksForTesting());
        // The last tab should still get thumbnail even though readbacks for other tabs are stuck.
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @DisabledTest(message = "crbug.com/981409")
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @DisabledTest(message = "crbug.com/981341")
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
            enterGTS();

            final int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
            final int targetIndex = switchToAnotherTab ? 1 - index : index;
            Tab targetTab =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(targetIndex);
            if (killBeforeSwitching) {
                WebContentsUtils.simulateRendererKilled(targetTab.getWebContents(), false);
            }

            if (switchToAnotherTab) {
                waitForCaptureRateControl();
            }
            int count = getCaptureCount();
            Espresso.onView(ViewMatchers.withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .perform(RecyclerViewActions.actionOnItemAtPosition(
                            targetIndex, ViewActions.click()));
            CriteriaHelper.pollUiThread(() -> {
                boolean doneHiding =
                        !mActivityTestRule.getActivity().getLayoutManager().overviewVisible();
                if (!doneHiding) {
                    // Before overview hiding animation is done, the tab index should not change.
                    Assert.assertEquals(
                            index, mActivityTestRule.getActivity().getCurrentTabModel().index());
                }
                return doneHiding;
            }, "Overview not hidden yet");
            int delta;
            if (switchToAnotherTab
                    && !TextUtils.equals(mActivityTestRule.getActivity()
                                                 .getCurrentWebContents()
                                                 .getLastCommittedUrl(),
                            NTP_URL)) {
                // Capture the original tab.
                delta = 1;
            } else {
                delta = 0;
            }
            checkCaptureCount(delta, count);
        }
        checkFinalCaptureCount(switchToAnotherTab, initCount);
        assertThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRestoredTabsDontFetch() throws Exception {
        prepareTabs(2, mUrl);
        TabSwitcher.TabListDelegate delegate =
                mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        int oldCount = delegate.getBitmapFetchCountForTesting();

        // Restart Chrome.
        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertEquals(3, mActivityTestRule.tabsCount(false));

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        Assert.assertEquals(0, delegate.getBitmapFetchCountForTesting() - oldCount);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testInvisibleTabsDontFetch() throws InterruptedException {
        // Open a few new tabs.
        final int count = mAllBitmaps.size();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        Assert.assertEquals(0, mAllBitmaps.size() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testInvisibleTabsDontFetchWarm() throws InterruptedException {
        // Get the GTS in the warm state.
        prepareTabs(2, NTP_URL);
        mRepeat = 2;
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mAllBitmaps.size();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        Assert.assertEquals(0, mAllBitmaps.size() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testInvisibleTabsDontFetchSoft() throws InterruptedException {
        // Get the GTS in the soft cleaned up state.
        prepareTabs(2, NTP_URL);
        mRepeat = 2;
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mAllBitmaps.size();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        Assert.assertEquals(0, mAllBitmaps.size() - count);
    }

    private void enterGTS() throws InterruptedException {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail)
            mActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());

        int count = getCaptureCount();
        waitForCaptureRateControl();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(true));
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        // Make sure the fading animation is done.
        int delta;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            // NTP is not invalidated, so no new captures.
            delta = 0;
        } else {
            // The final capture at StartSurfaceLayout#finishedShowing time.
            delta = 1;
            // TODO(wychen): refactor areAnimatorsEnabled() to a util class.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                // The faster capturing without writing back to cache.
                delta += 1;
            }
        }
        checkCaptureCount(delta, count);
        if (checkThumbnail) checkThumbnailsExist(currentTab);
    }

    private void checkFinalCaptureCount(boolean switchToAnotherTab, int initCount) {
        int expected;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            expected = 0;
        } else {
            expected = mRepeat;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                expected += mRepeat;
            }
            if (switchToAnotherTab) {
                expected += mRepeat;
            }
        }
        checkCaptureCount(expected, initCount);
    }

    private void checkCaptureCount(int expectedDelta, int initCount) {
        // TODO(wychen): With animation, the 2nd capture might be skipped if the 1st takes too long.
        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedDelta, () -> getCaptureCount() - initCount));
    }

    private void checkThumbnailsExist(Tab tab) {
        File etc1File = TabContentManager.getTabThumbnailFileEtc1(tab);
        CriteriaHelper.pollInstrumentationThread(etc1File::exists,
                "The thumbnail " + etc1File.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10, DEFAULT_POLLING_INTERVAL);

        File jpegFile = TabContentManager.getTabThumbnailFileJpeg(tab);
        CriteriaHelper.pollInstrumentationThread(jpegFile::exists,
                "The thumbnail " + jpegFile.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10, DEFAULT_POLLING_INTERVAL);
    }

    private int getCaptureCount() {
        return RecordHistogram.getHistogramTotalCountForTesting("Compositing.CopyFromSurfaceTime");
    }

    private void waitForCaptureRateControl() throws InterruptedException {
        // Needs to wait for at least |kCaptureMinRequestTimeMs| in order to capture another one.
        // TODO(wychen): find out why waiting is still needed after setting
        //               |kCaptureMinRequestTimeMs| to 0.
        Thread.sleep(2000);
    }

    private void assertThumbnailsAreReleased() {
        // Could not directly assert canAllBeGarbageCollected() because objects can be in Cleaner.
        CriteriaHelper.pollInstrumentationThread(() -> canAllBeGarbageCollected(mAllBitmaps));
    }

    private boolean canAllBeGarbageCollected(List<WeakReference<Bitmap>> bitmaps) {
        for (WeakReference<Bitmap> bitmap : bitmaps) {
            if (!GarbageCollectionTestUtils.canBeGarbageCollected(bitmap)) return false;
        }
        return true;
    }

    /**
     * Should be the same as {@link ValueAnimator#areAnimatorsEnabled}, which requires API level 26.
     * TODO(crbug.com/982018): put this interface in a place to share with
     * TabListContainerViewBinderTest.areAnimatorsEnabled.
     */
    private static boolean areAnimatorsEnabled() {
        // We default to assuming that animations are enabled in case ANIMATOR_DURATION_SCALE is not
        // defined.
        final float defaultScale = 1f;
        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, defaultScale);
        return !(durationScale == 0.0);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.chrome.browser.util.UrlConstants.NTP_URL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.animation.ValueAnimator;
import android.graphics.Bitmap;
import android.os.Build;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.ViewAssertion;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.View;

import org.junit.After;
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
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
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
    private TabSwitcher.TabListDelegate mTabListDelegate;
    private boolean mSkipAssertThumbnailsAreReleased;

    @Before
    public void setUp() {
        // After setUp, Chrome is launched and has one NTP.
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityFromLauncher();

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        mUrl = testServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 3;

        mTabListDelegate = mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());

        mActivityTestRule.getActivity().getTabContentManager().setCaptureMinRequestTimeForTesting(
                0);

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());
        // Only skip thumbnail releasing assertion when "warm" (large soft-cleanup-delay).
        mSkipAssertThumbnailsAreReleased = false;
    }

    @After
    public void tearDown() {
        if (!mSkipAssertThumbnailsAreReleased) assertThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromLiveTab() throws InterruptedException {
        // clang-format on
        assertEquals(0, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(0, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/991852 This test is flaky")
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        // clang-format on
        assertTrue(TabFeatureUtilities.isTabToGtsAnimationEnabled());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        // clang-format on
        assertEquals(10000, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
        mSkipAssertThumbnailsAreReleased = true;
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // TODO(crbug.com/997065#c8): remove SDK restriction.
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
        mSkipAssertThumbnailsAreReleased = true;
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        // clang-format on
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // TODO(crbug.com/997065#c8): remove SDK restriction.
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        // clang-format on
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);
    }

    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded.
     *
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     */
    private void prepareTabs(int numTabs, int numIncognitoTabs, @Nullable String url) {
        assertTrue(numTabs >= 1);
        assertTrue(numIncognitoTabs >= 0);

        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();
        assertEquals(1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        assertEquals(
                0, mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());

        if (numTabs == 1) {
            if (url != null) mActivityTestRule.loadUrl(url);
        } else {
            // When Chrome started, there is already one Tab created by default.
            createTabs(numTabs - 1, url, true, false);
        }
        if (numIncognitoTabs > 0) createTabs(numIncognitoTabs, url, true, true);

        assertEquals(numTabs,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        assertEquals(numIncognitoTabs,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    /**
     * When Chrome started, there is already one Tab created by default. This method is used to add
     * additional {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     * @param numTabs The number of tabs to create.
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *            be saved.
     * @param waitForLoading Whether wait for URL loading.
     * @param isIncognito Whether the tab is incognito tab.
     */
    private void createTabs(
            int numTabs, @Nullable String url, boolean waitForLoading, boolean isIncognito) {
        assertTrue(numTabs >= 1);

        if (url != null) mActivityTestRule.loadUrl(url);

        int previousTabCount = mActivityTestRule.getActivity()
                                       .getTabModelSelector()
                                       .getModel(isIncognito)
                                       .getCount();

        for (int i = 0; i < numTabs; i++) {
            TabModel previousTabModel =
                    mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
            int previousTabIndex = previousTabModel.index();
            Tab previousTab = previousTabModel.getTabAt(previousTabIndex);

            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), isIncognito, waitForLoading);

            if (url != null) mActivityTestRule.loadUrl(url);
            if (!waitForLoading) continue;

            TabModel currentTabModel =
                    mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
            int currentTabIndex = currentTabModel.index();

            boolean fixPendingReadbacks = mActivityTestRule.getActivity()
                                                  .getTabContentManager()
                                                  .getPendingReadbacksForTesting()
                    != 0;

            // When there are pending readbacks due to detached Tabs, try to fix it by switching
            // back to that tab.
            if (fixPendingReadbacks && previousTabIndex != TabModel.INVALID_TAB_INDEX) {
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() ->
                        previousTabModel.setIndex(previousTabIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }

            checkThumbnailsExist(previousTab);

            if (fixPendingReadbacks) {
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() -> currentTabModel.setIndex(
                        currentTabIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }
        }

        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(), null,
                null, WAIT_TIMEOUT_SECONDS * 10);

        assertEquals(numTabs + previousTabCount,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getModel(isIncognito)
                        .getCount());

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

        for (int i = 0; i < mRepeat; i++) {
            enterGTS();
            leaveGTS();
        }
        checkFinalCaptureCount(false, initCount);
    }

    @Test
    @MediumTest
    public void testGridToTabToCurrentNTP() throws InterruptedException {
        prepareTabs(1, 0, NTP_URL);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToOtherNTP() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToCurrentLive() throws InterruptedException {
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    // From https://stackoverflow.com/a/21505193
    private static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic") || Build.FINGERPRINT.startsWith("unknown")
                || Build.MODEL.contains("google_sdk") || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                || (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic"))
                || "google_sdk".equals(Build.PRODUCT);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToCurrentLiveDetached() throws Exception {
        // This works on emulators but not on real devices. See crbug.com/986047.
        if (!isEmulator()) return;

        for (int i = 0; i < 10; i++) {
            mActivityTestRule.loadUrl(mUrl);
            // Quickly create some tabs, navigate to web pages, and don't wait for thumbnail
            // capturing.
            createTabs(1, mUrl, false, false);
            // Hopefully we are in a state where some pending readbacks are stuck because their tab
            // is not attached to the view.
            if (mActivityTestRule.getActivity()
                            .getTabContentManager()
                            .getPendingReadbacksForTesting()
                    > 0) {
                break;
            }

            // Restart Chrome.
            // Although we're destroying the activity, the Application will still live on since its
            // in the same process as this test.
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getTabModelSelector().closeAllTabs());
            ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
            mActivityTestRule.startMainActivityOnBlankPage();
            assertEquals(1, mActivityTestRule.tabsCount(false));
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToCurrentLiveWithAnimation() throws InterruptedException {
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToOtherLive() throws InterruptedException {
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToOtherLiveWithAnimation() throws InterruptedException {
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        prepareTabs(2, 0, mUrl);
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
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .perform(RecyclerViewActions.actionOnItemAtPosition(targetIndex, click()));
            CriteriaHelper.pollUiThread(() -> {
                boolean doneHiding =
                        !mActivityTestRule.getActivity().getLayoutManager().overviewVisible();
                if (!doneHiding) {
                    // Before overview hiding animation is done, the tab index should not change.
                    assertEquals(
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
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRestoredTabsDontFetch() throws Exception {
        prepareTabs(2, 0, mUrl);
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Restart Chrome.
        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityOnBlankPage();
        assertEquals(3, mActivityTestRule.tabsCount(false));

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testInvisibleTabsDontFetch() throws InterruptedException {
        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/10000/cleanup-delay/10000"})
    public void testInvisibleTabsDontFetchWarm() throws InterruptedException {
        // Get the GTS in the warm state.
        prepareTabs(2, 0, NTP_URL);
        mRepeat = 2;
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
        mSkipAssertThumbnailsAreReleased = true;
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testInvisibleTabsDontFetchSoft() throws InterruptedException {
        // Get the GTS in the soft cleaned up state.
        prepareTabs(2, 0, NTP_URL);
        mRepeat = 2;
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    // clang-format off
    @DisabledTest(message = "http://crbug/1005865 - Test was previously flaky but only on bots."
            + "Was not locally reproducible. Disabling until verified that it's deflaked on bots.")
    public void testIncognitoEnterGts() throws InterruptedException {
        prepareTabs(1, 1, null);
        enterGTS();
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(1));

        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollInstrumentationThread(
                () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        enterGTS();
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(1));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testIncognitoToggle_tabCount() throws InterruptedException {
        mActivityTestRule.loadUrl(mUrl);

        // Prepare two incognito tabs and enter tab switcher.
        prepareTabs(1, 2, mUrl);
        enterGTS();
        onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(2));

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(false);
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .check(TabCountAssertion.havingTabCount(1));

            switchTabModel(true);
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .check(TabCountAssertion.havingTabCount(2));
        }
        leaveGTS();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testIncognitoToggle_thumbnailFetchCount() throws InterruptedException {
        mActivityTestRule.loadUrl(mUrl);
        int oldFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Prepare two incognito tabs and enter tab switcher.
        prepareTabs(1, 2, mUrl);
        enterGTS();

        int currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
        assertEquals(2, currentFetchCount - oldFetchCount);
        oldFetchCount = currentFetchCount;

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(false);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            assertEquals(1, currentFetchCount - oldFetchCount);
            oldFetchCount = currentFetchCount;

            switchTabModel(true);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            assertEquals(2, currentFetchCount - oldFetchCount);
            oldFetchCount = currentFetchCount;
        }
        leaveGTS();
    }

    private static class TabCountAssertion implements ViewAssertion {
        private int mExpectedCount;

        public static TabCountAssertion havingTabCount(int tabCount) {
            return new TabCountAssertion(tabCount);
        }

        public TabCountAssertion(int expectedCount) {
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView.Adapter adapter = ((RecyclerView) view).getAdapter();
            assertEquals(mExpectedCount, adapter.getItemCount());
        }
    }

    private void switchTabModel(boolean isIncognito) {
        assertTrue(isIncognito !=
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());

        onView(withContentDescription(
                isIncognito ? R.string.accessibility_tab_switcher_incognito_stack
                            : R.string.accessibility_tab_switcher_standard_stack)
        ).perform(click());

        CriteriaHelper.pollInstrumentationThread(() ->
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected()
                        == isIncognito);
    }

    private void enterGTS() throws InterruptedException {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            mActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());
        }

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

    private void leaveGTS() {
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        StartSurface startSurface = mStartSurfaceLayout.getStartSurfaceForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { startSurface.getController().onBackPressed(); });
        CriteriaHelper.pollInstrumentationThread(
                () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                "Overview not hidden yet", DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);
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
            if (!GarbageCollectionTestUtils.canBeGarbageCollected(bitmap)) {
                return false;
            }
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

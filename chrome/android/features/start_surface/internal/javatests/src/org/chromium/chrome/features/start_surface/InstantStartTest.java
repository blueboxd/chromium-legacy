// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.util.Base64;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.StreamUtil;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelMetadata;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0";
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    /**
     * Only launch Chrome without waiting for a current tab.
     * This test could not use {@link ChromeActivityTestRule#startMainActivityFromLauncher()}
     * because of its {@link org.chromium.chrome.browser.tab.Tab} dependency.
     */
    private void startMainActivityFromLauncher() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.startActivityCompletely(intent);
    }

    public static Bitmap createThumbnailBitmapAndWriteToFile(int tabId) {
        final Bitmap thumbnailBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        try {
            File thumbnailFile = TabContentManager.getTabThumbnailFileJpeg(tabId);
            if (thumbnailFile.exists()) {
                thumbnailFile.delete();
            }
            Assert.assertFalse(thumbnailFile.exists());

            FileOutputStream thumbnailFileOutputStream = new FileOutputStream(thumbnailFile);
            thumbnailBitmap.compress(Bitmap.CompressFormat.JPEG, 100, thumbnailFileOutputStream);
            thumbnailFileOutputStream.flush();
            thumbnailFileOutputStream.close();

            Assert.assertTrue(thumbnailFile.exists());
        } catch (IOException e) {
            e.printStackTrace();
        }
        return thumbnailBitmap;
    }

    private void createCorruptedTabStateFile() throws IOException {
        File stateFile =
                new File(TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                        TabbedModeTabPersistencePolicy.getStateFileName(0));
        FileOutputStream output = new FileOutputStream(stateFile);
        output.write(0);
        output.close();
    }

    /**
     * Create a file so that a TabState can be restored later.
     * @param tabId the Tab ID
     * @param encrypted for Incognito mode
     */
    private static void saveTabState(int tabId, boolean encrypted) {
        File file = TabState.getTabStateFile(
                TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(), tabId,
                encrypted);
        writeFile(file, M26_GOOGLE_COM.encodedTabState);

        TabState tabState = TabState.restoreTabState(file, false);
        tabState.rootId = PseudoTab.fromTabId(tabId).getRootId();
        TabState.saveState(file, tabState, encrypted);
    }

    private static void writeFile(File file, String data) {
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (Exception e) {
            assert false : "Failed to create " + file;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    /**
     * Create all the files so that tab models can be restored
     * @param tabIds all the Tab IDs in the normal tab model.
     */
    public static void createTabStateFile(int[] tabIds) throws IOException {
        createTabStateFile(tabIds, 0);
    }

    /**
     * Create all the files so that tab models can be restored
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param selectedIndex the selected index of normal tab model.
     */
    public static void createTabStateFile(int[] tabIds, int selectedIndex) throws IOException {
        TabModelMetadata normalInfo = new TabModelMetadata(selectedIndex);
        for (int tabId : tabIds) {
            normalInfo.ids.add(tabId);
            normalInfo.urls.add("");

            saveTabState(tabId, false);
        }
        TabModelMetadata incognitoInfo = new TabModelMetadata(0);

        byte[] listData = TabPersistentStore.serializeMetadata(normalInfo, incognitoInfo, null);

        File stateFile =
                new File(TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                        TabbedModeTabPersistencePolicy.getStateFileName(0));
        FileOutputStream output = new FileOutputStream(stateFile);
        output.write(listData);
        output.close();
    }

    private void startAndWaitNativeInitialization() {
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().startDelayedNativeInitializationForTests());
        CriteriaHelper.pollUiThread(() -> {
            return mActivityTestRule.getActivity()
                            .getTabModelSelector()
                            .getTabModelFilterProvider()
                            .getCurrentTabModelFilter()
                    != null
                    && mActivityTestRule.getActivity()
                               .getTabModelSelector()
                               .getTabModelFilterProvider()
                               .getCurrentTabModelFilter()
                               .isTabModelRestored();
        });
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }

    /**
     * Test TabContentManager is able to fetch thumbnail jpeg files before native is initialized.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:allow_to_refetch/true/thumbnail_aspect_ratio/2.0"})
    public void fetchThumbnailsPreNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertTrue(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION.getValue());

        int tabId = 0;
        mThumbnailFetchCount = 0;
        Callback<Bitmap> thumbnailFetchListener = (bitmap) -> {
            mThumbnailFetchCount++;
            mBitmap = bitmap;
        };

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(mActivityTestRule.getActivity().getTabContentManager(),
                                Matchers.notNullValue()));

        TabContentManager tabContentManager =
                mActivityTestRule.getActivity().getTabContentManager();

        final Bitmap thumbnailBitmap = createThumbnailBitmapAndWriteToFile(tabId);
        tabContentManager.getTabThumbnailWithCallback(tabId, thumbnailFetchListener, false, false);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mThumbnailFetchCount > 0;
            }
        });

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        Assert.assertEquals(1, mThumbnailFetchCount);
        Bitmap fetchedThumbnail = mBitmap;
        Assert.assertEquals(thumbnailBitmap.getByteCount(), fetchedThumbnail.getByteCount());
        Assert.assertEquals(thumbnailBitmap.getWidth(), fetchedThumbnail.getWidth());
        Assert.assertEquals(thumbnailBitmap.getHeight(), fetchedThumbnail.getHeight());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS})
    public void layoutManagerChromePhonePreNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(()
                                            -> Assert.assertTrue(mActivityTestRule.getActivity()
                                                                         .getLayoutManager()
                                                                         .overviewVisible()));

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void startSurfaceSinglePanePreNativeAndWithNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(()
                                            -> Assert.assertTrue(mActivityTestRule.getActivity()
                                                                         .getLayoutManager()
                                                                         .overviewVisible()));

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);

        StartSurfaceCoordinator startSurfaceCoordinator =
                (StartSurfaceCoordinator) ((ChromeTabbedActivity) mActivityTestRule.getActivity())
                        .getStartSurface();
        Assert.assertTrue(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isInitializedWithNativeForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurfaceCoordinator.getController().setOverviewState(
                    OverviewModeState.SHOWN_TABSWITCHER);
        });
        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertTrue(
                                startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting()));

        // Initializes native.
        startAndWaitNativeInitialization();
        Assert.assertFalse(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());
        Assert.assertTrue(startSurfaceCoordinator.isInitializedWithNativeForTesting());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void willInitNativeOnTabletTest() {
        Assert.assertTrue(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        CriteriaHelper.pollUiThread(
                ()
                        -> Assert.assertThat(mActivityTestRule.getActivity().getLayoutManager(),
                                Matchers.notNullValue()));

        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromeTablet.class);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    public void testShouldShowStartSurfaceAsTheHomePagePreNative() {
        // clang-format on
        Assert.assertTrue(StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled());
        Assert.assertFalse(TextUtils.isEmpty(HomepageManager.getHomepageUri()));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) ReturnToChromeExperimentsUtil::shouldShowStartSurfaceAsTheHomePage);
    }

    private void showOverview() {
        AtomicReference<StartSurface> startSurface = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabManagementDelegate tabManagementDelegate = TabManagementModuleProvider.getDelegate();
            startSurface.set(
                    tabManagementDelegate.createStartSurface(mActivityTestRule.getActivity()));
            startSurface.get().getController().enableRecordingFirstMeaningfulPaint(0);
            startSurface.get().getController().showOverview(false);
        });
        Assert.assertTrue(startSurface.get().getController().overviewVisible());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    public void renderTabSwitcher_NoStateFile() throws IOException {
        startMainActivityFromLauncher();
        showOverview();

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    public void renderTabSwitcher_CorruptedStateFile() throws IOException {
        createCorruptedTabStateFile();
        startMainActivityFromLauncher();
        showOverview();

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_empty");
    }

    private boolean allCardsHaveThumbnail(RecyclerView recyclerView) {
        RecyclerView.Adapter adapter = recyclerView.getAdapter();
        assert adapter != null;
        for (int i = 0; i < adapter.getItemCount(); i++) {
            RecyclerView.ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder != null) {
                ImageView thumbnail = viewHolder.itemView.findViewById(R.id.tab_thumbnail);
                BitmapDrawable drawable = (BitmapDrawable) thumbnail.getDrawable();
                Bitmap bitmap = drawable.getBitmap();
                if (bitmap == null) return false;
            }
        }
        return true;
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    public void renderTabSwitcher() throws IOException, InterruptedException {
        createTabStateFile(new int[] {0, 1, 2});
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        createThumbnailBitmapAndWriteToFile(2);
        TabAttributeCache.setTitleForTesting(0, "title");
        TabAttributeCache.setTitleForTesting(1, "漢字");
        TabAttributeCache.setTitleForTesting(2, "اَلْعَرَبِيَّةُ");

        // Must be after createTabStateFile() to read these files.
        startMainActivityFromLauncher();
        showOverview();

        RecyclerView recyclerView =
                mActivityTestRule.getActivity().findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(
                Criteria.equals(true, () -> allCardsHaveThumbnail(recyclerView)));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_3tabs");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(3,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        // TODO(crbug.com/1065314): find a better way to wait for a stable rendering.
        Thread.sleep(2000);
        // The titles on the tab cards changes to "Google" because we use M26_GOOGLE_COM.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_3tabs_postNative");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void renderTabGroups() throws IOException, InterruptedException {
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        createThumbnailBitmapAndWriteToFile(2);
        createThumbnailBitmapAndWriteToFile(3);
        createThumbnailBitmapAndWriteToFile(4);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        TabAttributeCache.setRootIdForTesting(2, 0);
        TabAttributeCache.setRootIdForTesting(3, 3);
        TabAttributeCache.setRootIdForTesting(4, 3);
        // createTabStateFile() has to be after setRootIdForTesting() to get root IDs.
        createTabStateFile(new int[] {0, 1, 2, 3, 4});

        // Must be after createTabStateFile() to read these files.
        startMainActivityFromLauncher();
        showOverview();

        RecyclerView recyclerView =
                mActivityTestRule.getActivity().findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(
                Criteria.equals(true, () -> allCardsHaveThumbnail(recyclerView)));
        // TODO(crbug.com/1065314): Tab group cards should not have favicons.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_tabGroups");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(5,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        Assert.assertEquals(2,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getCount());
        Assert.assertEquals(3,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getRelatedTabList(2)
                        .size());
        // TODO(crbug.com/1065314): find a better way to wait for a stable rendering.
        Thread.sleep(2000);
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_tabGroups_postNative");
    }
}

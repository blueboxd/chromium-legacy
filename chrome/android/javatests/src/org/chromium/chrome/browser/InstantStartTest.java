// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS + "/0";
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        startMainActivityFromLauncher();
    }

    private void startMainActivityFromLauncher() {
        // Only launch Chrome.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.startActivityCompletely(intent);
    }

    private Bitmap createThumbnailBitmapAndWriteToFile(int tabId) {
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
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return thumbnailBitmap;
    }

    /**
     * Test TabContentManager is able to fetch thumbnail jpeg files before native is initialized.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION)
    public void fetchThumbnailsPreNativeTest() {
        int tabId = 0;
        mThumbnailFetchCount = 0;
        Callback<Bitmap> thumbnailFetchListener = (bitmap) -> {
            mThumbnailFetchCount++;
            mBitmap = bitmap;
        };

        CriteriaHelper.pollUiThread(Criteria.checkThat(
                ()
                        -> mActivityTestRule.getActivity().getTabContentManager(),
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
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "enable-features=" + ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            "force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void layoutManagerChromePhonePreNativeTest() {
        // clang-format on
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(Criteria.checkThat(
                () -> mActivityTestRule.getActivity().getLayoutManager(), Matchers.notNullValue()));

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void willInitNativeOnTabletTest() {
        Assert.assertTrue(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        CriteriaHelper.pollUiThread(Criteria.checkThat(
                () -> mActivityTestRule.getActivity().getLayoutManager(), Matchers.notNullValue()));

        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromeTablet.class);
    }
}
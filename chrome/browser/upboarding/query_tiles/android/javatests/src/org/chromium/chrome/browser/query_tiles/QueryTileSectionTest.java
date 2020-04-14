// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the query tiles section on new tab page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class QueryTileSectionTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        TileProviderFactory.setTileProviderForTesting(new TestTileProvider());
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl("chrome-native://newtab/");
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.QUERY_TILES)
    public void testSimpleTest() throws Exception {
        onView(withId(R.id.query_tiles)).check(matches(isDisplayed()));
    }

    private static class TestTileProvider implements TileProvider {
        private List<Tile> mTiles = new ArrayList<>();

        private TestTileProvider() {
            Tile tile = new Tile("1", "Tile 1", "Tile 1", "Tile 1", null);
            mTiles.add(tile);
        }

        @Override
        public void getQueryTiles(Callback<List<Tile>> callback) {
            callback.onResult(mTiles);
        }

        @Override
        public void getVisuals(String id, Callback<List<Bitmap>> callback) {
            callback.onResult(null);
        }
    }
}

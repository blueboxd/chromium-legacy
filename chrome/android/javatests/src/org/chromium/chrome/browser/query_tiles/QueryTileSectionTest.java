// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.support.test.espresso.action.ViewActions;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TileProvider;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the query tiles section on new tab page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.QUERY_TILES)
public class QueryTileSectionTest {
    private static final String SEARCH_URL_PATTERN = "https://www.google.com/search?q=";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private TestTileProvider mTileProvider;

    @Before
    public void setUp() {
        mTileProvider = new TestTileProvider();
        TileProviderFactory.setTileProviderForTesting(mTileProvider);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void testShowQueryTileSection() throws Exception {
        // Verify top level tiles show up on init.
        onView(withId(R.id.query_tiles)).check(matches(isDisplayed()));
        onView(withText(mTileProvider.getTileAt(0).displayTitle)).check(matches(isDisplayed()));
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testSearchWithLastLevelTile() throws Exception {
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);

        // Click on first level tile.
        onView(withText(tile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        // Click on last level tile. We should navigate to SRP with the query text.
        onView(withText(subtile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        waitForSearchResultsPage();
        Assert.assertTrue(getTabUrl().contains(subtile.queryText));
    }

    @Test
    @SmallTest
    public void testSearchWithFirstLevelTile() throws Exception {
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);

        // Click first level tile. Chip should show up.
        onView(withText(tile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withText(subtile.displayTitle)).check(matches(isDisplayed()));

        // Click on the chip. SRP should show up with first level query text.
        onView(withId(R.id.query_tiles_chip)).perform(ViewActions.click());
        waitForSearchResultsPage();
        Assert.assertTrue(getTabUrl().contains(tile.queryText));
    }

    @Test
    @SmallTest
    public void testChipVisibilityOnFakeBox() throws Exception {
        QueryTile tile = mTileProvider.getTileAt(0);

        // No chip should be shown by default.
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));

        // Chip shows up when first level tile clicked.
        onView(withText(tile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withId(R.id.query_tiles_chip)).check(matches(isDisplayed()));

        // Chip disappears on hitting clear button.
        clearSelectedChip();
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testClearingSelectedTileBringsBackTopLevelTiles() throws Exception {
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);

        // Navigate to second level tile.
        onView(withText(tile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withText(subtile.displayTitle)).check(matches(isDisplayed()));

        // Clear selected chip. We should be back at top level tiles.
        clearSelectedChip();
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
        onView(withText(tile.displayTitle)).check(matches(isDisplayed()));
        onView(withText(subtile.displayTitle)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testFocusOmniboxWithZeroSuggest() throws Exception {
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);

        onView(withId(R.id.query_tiles)).check(matches(isDisplayed()));
        onView(withText(mTileProvider.getTileAt(0).displayTitle)).check(matches(isDisplayed()));

        // Click on the search box. Omnibox should show up.
        onView(withId(R.id.search_box_text)).perform(ViewActions.click());
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        // OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);
        Assert.assertTrue(urlBar.getText().toString().isEmpty());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "See https://crbug.com/1075471")
    public void testFocusOmniboxWithFirstLevelQueryText() throws Exception {
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);

        // Navigate to second level tiles.
        onView(withText(tile.displayTitle))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withText(subtile.displayTitle)).check(matches(isDisplayed()));
        onView(withId(R.id.query_tiles_chip)).check(matches(isDisplayed()));

        // Click on the search box. Omnibox should show up with first level query text.
        onView(withId(R.id.search_box_text)).perform(ViewActions.click());
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        // OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);
        Assert.assertTrue(urlBar.getText().toString().contains(tile.queryText));
    }

    private void clearSelectedChip() {
        onView(withId(R.id.chip_cancel_btn)).perform(ViewActions.click());
    }

    private String getTabUrl() {
        return mActivityTestRule.getActivity().getActivityTab().getUrl().getValidSpecOrEmpty();
    }

    private void waitForSearchResultsPage() {
        CriteriaHelper.pollUiThread(
                new Criteria("The SRP was never loaded. " + mTab.getUrl().getValidSpecOrEmpty()) {
                    @Override
                    public boolean isSatisfied() {
                        return mTab.getUrl().getValidSpecOrEmpty().contains(SEARCH_URL_PATTERN);
                    }
                });
    }

    private static class TestTileProvider implements TileProvider {
        private List<QueryTile> mTiles = new ArrayList<>();

        private TestTileProvider() {
            List<QueryTile> children = new ArrayList<>();
            children.add(new QueryTile("tile1_1", "Tile 1_1", "Tile 1_1", "Tile_1_1_Query",
                    new String[] {"url1_1"}, null));
            QueryTile tile = new QueryTile(
                    "1", "Tile 1", "Tile 1", "Tile_1_Query", new String[] {"url1"}, children);
            mTiles.add(tile);
        }

        @Override
        public void getQueryTiles(Callback<List<QueryTile>> callback) {
            callback.onResult(mTiles);
        }

        public QueryTile getTileAt(int index) {
            assert index < mTiles.size();
            return mTiles.get(index);
        }

        public QueryTile getTileAt(int index, int childIndex) {
            assert index < mTiles.size() && childIndex < mTiles.get(index).children.size();
            return mTiles.get(index).children.get(childIndex);
        }
    }
}

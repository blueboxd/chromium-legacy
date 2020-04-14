// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.util.List;

/**
 * Represents the query tiles section on the new tab page. Abstracts away the general tasks related
 * to initializing and fetching data for the UI and making decisions whether to show or hide this
 * section.
 */
public class QueryTileSection {
    private static final String QUERY_TILES_SHORTEN_MOST_VISITED_TILES_FOR_SMALL_SCREEN =
            "shorten_most_visited_tiles_for_small_screen";
    private static final int SMALL_SCREEN_HEIGHT_THRESHOLD_DP = 600;

    private final ViewGroup mQueryTileSectionView;
    private final SearchBoxCoordinator mSearchBoxCoordinator;
    private final Callback<String> mSubmitQueryCallback;
    private QueryTileCoordinator mQueryTileCoordinator;
    private TileProvider mTileProvider;

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView,
            SearchBoxCoordinator searchBoxCoordinator, Profile profile,
            Callback<String> performSearchQueryCallback) {
        mQueryTileSectionView = queryTileSectionView;
        mSearchBoxCoordinator = searchBoxCoordinator;
        mSubmitQueryCallback = performSearchQueryCallback;
        if (!isFeatureEnabled()) return;

        mTileProvider = TileProviderFactory.getForProfile(profile);
        mQueryTileCoordinator = QueryTileCoordinatorFactory.create(
                mQueryTileSectionView.getContext(), this::onTileClicked, this::getVisuals);
        mQueryTileSectionView.addView(mQueryTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        onTileClicked(null);
    }

    private void onTileClicked(Tile tile) {
        if (tile == null) {
            mTileProvider.getQueryTiles(this::setTiles);
        } else {
            boolean isLastLevelTile = tile.children.isEmpty();
            setTiles(tile.children);
            if (isLastLevelTile) {
                mSubmitQueryCallback.onResult(tile.queryText);
            } else {
                // TODO(shaktisahu): Show chip on fakebox;
            }
        }
    }

    private void setTiles(List<Tile> tiles) {
        mQueryTileCoordinator.setTiles(tiles);
        mQueryTileSectionView.setVisibility(tiles.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void getVisuals(Tile tile, Callback<List<Bitmap>> callback) {
        mTileProvider.getVisuals(tile.id, callback);
    }

    /**
     * @return Whether the screen height is small. Used for shortening the most visited tiles
     *         section on NTP so that feed is still visible above the fold.
     */
    public boolean shouldConsiderAsSmallScreen() {
        if (!isFeatureEnabled()) return false;
        boolean shortenMostVisitedTiles = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.QUERY_TILES,
                QUERY_TILES_SHORTEN_MOST_VISITED_TILES_FOR_SMALL_SCREEN, false);
        if (!shortenMostVisitedTiles) return false;

        DisplayAndroid display =
                DisplayAndroid.getNonMultiDisplay(mQueryTileSectionView.getContext());
        int screenHeightDp = DisplayUtil.pxToDp(display, display.getDisplayHeight());
        return screenHeightDp < SMALL_SCREEN_HEIGHT_THRESHOLD_DP;
    }

    private static boolean isFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES);
    }
}

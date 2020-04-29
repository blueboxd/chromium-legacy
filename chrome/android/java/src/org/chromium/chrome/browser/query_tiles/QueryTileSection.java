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
import org.chromium.chrome.browser.ntp.search.SearchBoxChipDelegate;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinatorFactory;
import org.chromium.components.browser_ui.widget.image_tiles.TileConfig;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TileProvider;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.util.ArrayList;
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
    private static final String UMA_PREFIX = "NTP.QueryTiles";

    private final ViewGroup mQueryTileSectionView;
    private final SearchBoxCoordinator mSearchBoxCoordinator;
    private final Callback<String> mSubmitQueryCallback;
    private ImageTileCoordinator mTileCoordinator;
    private TileProvider mTileProvider;

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView,
            SearchBoxCoordinator searchBoxCoordinator, Profile profile,
            Callback<String> performSearchQueryCallback) {
        mQueryTileSectionView = queryTileSectionView;
        mSearchBoxCoordinator = searchBoxCoordinator;
        mSubmitQueryCallback = performSearchQueryCallback;
        if (!isFeatureEnabled()) return;

        TileProviderFactory.setFakeTileProvider(new FakeTileProvider());
        mTileProvider = TileProviderFactory.getForProfile(profile);
        TileConfig tileConfig = new TileConfig.Builder().setUmaPrefix(UMA_PREFIX).build();
        mTileCoordinator = ImageTileCoordinatorFactory.create(mQueryTileSectionView.getContext(),
                tileConfig, this::onTileClicked, this::getVisuals);
        mQueryTileSectionView.addView(mTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        reloadTiles();
    }

    private void onTileClicked(ImageTile tile) {
        QueryTile queryTile = (QueryTile) tile;
        boolean isLastLevelTile = queryTile.children.isEmpty();
        if (isLastLevelTile) {
            mSubmitQueryCallback.onResult(queryTile.queryText);
            return;
        }

        setTiles(queryTile.children);
        showQueryChip(queryTile);
    }

    private void showQueryChip(QueryTile queryTile) {
        mSearchBoxCoordinator.setChipText(queryTile.queryText);
        mSearchBoxCoordinator.setChipDelegate(new SearchBoxChipDelegate() {
            @Override
            public void onChipClicked() {
                mSubmitQueryCallback.onResult(queryTile.queryText);
            }

            @Override
            public void onCancelClicked() {
                mSearchBoxCoordinator.setChipText(null);
                reloadTiles();
            }

            @Override
            public void getChipIcon(Callback<Bitmap> callback) {
                mTileProvider.getVisuals(queryTile.id, bitmaps -> {
                    Bitmap bitmap = bitmaps != null && !bitmaps.isEmpty() ? bitmaps.get(0) : null;
                    callback.onResult(bitmap);
                });
            }
        });
    }

    private void reloadTiles() {
        mTileProvider.getQueryTiles(this::setTiles);
    }

    private void setTiles(List<QueryTile> tiles) {
        mTileCoordinator.setTiles(new ArrayList<>(tiles));
        mQueryTileSectionView.setVisibility(tiles.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
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

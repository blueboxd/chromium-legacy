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

import java.util.List;

/**
 * Represents the query tiles section on the new tab page. Abstracts away the general tasks related
 * to initializing and fetching data for the UI and making decisions whether to show or hide this
 * section.
 */
public class QueryTileSection {
    private final ViewGroup mQueryTileSectionView;
    private final SearchBoxCoordinator mSearchBoxCoordinator;
    private QueryTileCoordinator mQueryTileCoordinator;
    private TileProvider mTileProvider;

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView,
            SearchBoxCoordinator searchBoxCoordinator, Profile profile) {
        mQueryTileSectionView = queryTileSectionView;
        mSearchBoxCoordinator = searchBoxCoordinator;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)) return;

        mTileProvider = TileProviderFactory.getForProfile(profile);
        mQueryTileCoordinator = QueryTileCoordinatorFactory.create(
                mQueryTileSectionView.getContext(), this::onTileClicked, this::getVisuals);
        mQueryTileSectionView.addView(mQueryTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        onTileClicked(null);
    }

    private void onTileClicked(Tile tile) {
        mTileProvider.getQueryTiles(tiles -> {
            mQueryTileCoordinator.setTiles(tiles);
            mQueryTileSectionView.setVisibility(tiles.isEmpty() ? View.GONE : View.VISIBLE);
        });

        if (tile != null) mSearchBoxCoordinator.setSearchText(tile.queryText);
    }

    private void getVisuals(Tile tile, Callback<List<Bitmap>> callback) {
        mTileProvider.getVisuals(tile.id, callback);
    }
}
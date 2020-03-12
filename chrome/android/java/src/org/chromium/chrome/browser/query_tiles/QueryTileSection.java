// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Represents the query tiles section on the new tab page. Abstracts away the general tasks related
 * to initializing and fetching data for the UI and making decisions whether to show or hide this
 * section.
 */
public class QueryTileSection {
    private final ViewGroup mQueryTileSectionView;
    private TileProvider mTileProvider;
    private QueryTileCoordinator mQueryTileCoordinator;

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView, Profile profile) {
        mQueryTileSectionView = queryTileSectionView;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)) return;

        mTileProvider = TileProviderFactory.getForProfile(profile);
        mQueryTileCoordinator = QueryTileCoordinatorFactory.create(
                mQueryTileSectionView.getContext(), mTileProvider, this::onQueryTilesChanged);
        mQueryTileSectionView.addView(mQueryTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
    }

    private void onQueryTilesChanged(boolean hasTiles) {
        mQueryTileSectionView.setVisibility(hasTiles ? View.VISIBLE : View.GONE);
    }
}

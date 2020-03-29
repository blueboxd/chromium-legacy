// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles.list;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.query_tiles.QueryTileCoordinator;
import org.chromium.chrome.browser.query_tiles.Tile;

import java.util.List;

/**
 * The top level coordinator for the query tiles UI.
 */
public class QueryTileCoordinatorImpl implements QueryTileCoordinator {
    private final TileListModel mModel;
    private final TileListView mView;

    /** Constructor. */
    public QueryTileCoordinatorImpl(Context context, Callback<Tile> tileClickCallback,
            TileVisualsProvider visualsProvider) {
        mModel = new TileListModel();
        mView = new TileListView(context, mModel);

        mModel.getProperties().set(TileListProperties.CLICK_CALLBACK, tileClickCallback);
        mModel.getProperties().set(TileListProperties.VISUALS_CALLBACK, visualsProvider);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public void setTiles(List<Tile> tiles) {
        mModel.set(tiles);
    }
}

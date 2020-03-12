// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles.list;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.query_tiles.QueryTileCoordinator;
import org.chromium.chrome.browser.query_tiles.Tile;
import org.chromium.chrome.browser.query_tiles.TileProvider;

/**
 * The top level coordinator for the query tiles UI.
 */
public class QueryTileCoordinatorImpl implements QueryTileCoordinator {
    private final TileListModel mModel;
    private final TileListView mView;
    private final TileProvider mTileProvider;
    private final Callback<Boolean> mVisibilityCallback;

    /** Constructor. */
    public QueryTileCoordinatorImpl(
            Context context, TileProvider tileProvider, Callback<Boolean> visibilityCallback) {
        mTileProvider = tileProvider;
        mVisibilityCallback = visibilityCallback;
        mModel = new TileListModel();
        mView = new TileListView(context, mModel);

        mModel.getProperties().set(TileListProperties.CLICK_CALLBACK, this::onQueryTileClicked);
        mModel.getProperties().set(TileListProperties.VISUALS_CALLBACK, this::getVisuals);
        onQueryTileClicked(null);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    private void onQueryTileClicked(Tile tile) {
        if (tile != null) {
            mModel.set(tile.children);
        } else {
            mTileProvider.getQueryTiles(tiles -> {
                mModel.set(tiles);
                mVisibilityCallback.onResult(!tiles.isEmpty());
            });
        }
    }

    private void getVisuals(Tile tile, Callback<Bitmap> callback) {
        mTileProvider.getThumbnail(tile.id, callback);
    }
}

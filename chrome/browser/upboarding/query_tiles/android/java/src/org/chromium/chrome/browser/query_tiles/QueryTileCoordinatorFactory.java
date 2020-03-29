// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.query_tiles.QueryTileCoordinator.TileVisualsProvider;
import org.chromium.chrome.browser.query_tiles.list.QueryTileCoordinatorImpl;

/**
 * Factory to create {@link QueryTileCoordinator} instances.
 */
public class QueryTileCoordinatorFactory {
    /**
     * Creates a {@link QueryTileCoordinator}.
     * @param context The context associated with the current activity.
     * @param tileClickCallback A callback to be invoked when a tile is clicked.
     * @return A {@link QueryTileCoordinator}.
     */
    public static QueryTileCoordinator create(Context context, Callback<Tile> tileClickCallback,
            TileVisualsProvider tileVisualsProvider) {
        return new QueryTileCoordinatorImpl(context, tileClickCallback, tileVisualsProvider);
    }
}
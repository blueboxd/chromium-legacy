// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.query_tiles.list.QueryTileCoordinatorImpl;

/**
 * Factory to create {@link QueryTileCoordinator} instances.
 */
public class QueryTileCoordinatorFactory {
    /**
     * Creates a {@link QueryTileCoordinator}.
     * @param context The context associated with the current activity.
     * @param tileProvider The {@link TileProvider} to provide tiles.
     * @param visibilityCallback A callback to show/hide the query tile section
     * @return A {@link QueryTileCoordinator}.
     */
    public static QueryTileCoordinator create(
            Context context, TileProvider tileProvider, Callback<Boolean> visibilityCallback) {
        return new QueryTileCoordinatorImpl(context, tileProvider, visibilityCallback);
    }
}

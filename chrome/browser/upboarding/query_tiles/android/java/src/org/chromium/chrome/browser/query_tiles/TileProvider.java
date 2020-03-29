// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Java interface for interacting with the native query tile service. Responsible for initializing
 * and fetching data fo be shown on the UI.
 */
public interface TileProvider {
    /**
     * Called to retrieve all the tiles.
     * @param callback The {@link Callback} to be notified on completion. Returns an empty list if
     *         no tiles are found.
     */
    void getQueryTiles(Callback<List<Tile>> callback);

    /**
     * Called to retrieve visuals for the given tile id.
     * @param id The ID for a given tile.
     * @param callback The {@link Callback} to be run after fetching the thumbnail. Returns null if
     *         no visuals were found.
     */
    void getVisuals(String id, Callback<List<Bitmap>> callback);
}
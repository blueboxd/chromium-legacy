// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.util.List;

/**
 * Helper class to log metrics for various user actions associated with query tiles.
 */
public class TileUmaLogger {
    private final String mHistogramPrefix;
    private List<QueryTile> mTiles;

    public TileUmaLogger(String histogramPrefix) {
        mHistogramPrefix = histogramPrefix;
    }

    public void recordTilesLoaded(List<QueryTile> tiles) {
        mTiles = tiles;
        RecordHistogram.recordCountHistogram(
                "Search." + mHistogramPrefix + ".TileCount", mTiles.size());
    }

    public void recordTileClicked(QueryTile tile) {
        assert tile != null;

        boolean isTopLevel = isTopLevelTile(tile.id);
        RecordHistogram.recordBooleanHistogram(
                "Search." + mHistogramPrefix + ".Tile.Clicked.IsTopLevel", isTopLevel);

        int tileUmaId = getTileUmaId(tile.id);
        RecordHistogram.recordSparseHistogram(
                "Search." + mHistogramPrefix + ".Tile.Clicked", tileUmaId);
    }

    public void recordSearchButtonClicked(QueryTile tile) {
        int tileUmaId = getTileUmaId(tile.id);
        RecordHistogram.recordSparseHistogram(
                "Search.QueryTiles.NTP.Chip.SearchClicked", tileUmaId);
    }

    public void recordChipCleared() {
        RecordUserAction.record("Search.QueryTiles.NTP.FakeSearchBox.Chip.Cleared");
    }

    private boolean isTopLevelTile(String id) {
        for (QueryTile tile : mTiles) {
            if (tile.id.equals(id)) return true;
        }
        return false;
    }

    /**
     * Returns a UMA ID for the tile which will be used to identify the tile for metrics purposes.
     * The UMA ID is generated from the position of the tile in the query tile tree.
     * Top level tiles will be numbered 1,2,3... while the next level tile start
     * from 101 (101, 102, ... etc), 201 (201, 202, ... etc), and so on.
     */
    private int getTileUmaId(String tileId) {
        assert !TextUtils.isEmpty(tileId);

        for (int i = 0; i < mTiles.size(); i++) {
            int found = search(mTiles.get(i), tileId, i + 1);
            if (found != -1) return found;
        }

        return -1;
    }

    private int search(QueryTile tile, String id, int startPosition) {
        for (int i = 0; i < tile.children.size(); i++) {
            QueryTile child = tile.children.get(i);
            if (id.equals(child.id)) return startPosition + i;
            int found = search(child, id, startPosition * 100);
            if (found != -1) return found;
        }
        return -1;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;

import java.util.List;

/**
 * The top level coordinator for the tiles UI.
 */
class TileCoordinatorImpl implements ImageTileCoordinator {
    private final TileListModel mModel;
    private final TileListView mView;
    private final TileMediator mMediator;

    /** Constructor. */
    public TileCoordinatorImpl(Context context, TileConfig config,
            Callback<ImageTile> tileClickCallback, TileVisualsProvider visualsProvider) {
        mModel = new TileListModel();
        mView = new TileListView(context, config, mModel);
        mMediator = new TileMediator(config, mModel, tileClickCallback, visualsProvider);
    }

    @Override
    public View getView() {
        return mView.getView();
    }

    @Override
    public void setTiles(List<ImageTile> tiles) {
        mModel.set(tiles);
        mView.scrollToPosition(0);
    }
}

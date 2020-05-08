// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinatorFactory;
import org.chromium.components.browser_ui.widget.image_tiles.TileConfig;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Wrapper around {@link ImageTileCoordinator} that shows query tiles in omnibox suggestions.
 * Responsible for view creation, and wiring necessary dependencies for functioning of the widget.
 */
public class OmniboxQueryTileCoordinator {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;
    private static final String UMA_PREFIX = "QueryTiles.Omnibox";

    private final Context mContext;
    private final Callback<QueryTile> mSelectionCallback;
    private ImageTileCoordinator mTileCoordinator;
    private ImageFetcher mImageFetcher;
    private Integer mTileWidth;

    /**
     * Constructor.
     * @param context The associated {@link Context}.
     * @param selectionCallback The callback to be invoked on tile selection.
     */
    public OmniboxQueryTileCoordinator(Context context, Callback<QueryTile> selectionCallback) {
        mContext = context;
        mSelectionCallback = selectionCallback;
    }

    /** Called to set the list of query tiles to be displayed in the suggestion. */
    public void setTiles(List<QueryTile> tiles) {
        getTileCoordinator().setTiles(new ArrayList<>(tiles));
    }

    /** Called to clean up resources used by this class. */
    public void destroy() {
        if (mImageFetcher != null) mImageFetcher.destroy();
        mImageFetcher = null;
    }

    /** @return The query tiles suggestion view to be shown in the autocomplete suggestions list. */
    public ViewGroup createView(Context context) {
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        ViewGroup suggestionView = (ViewGroup) inflater.inflate(
                org.chromium.chrome.R.layout.omnibox_query_tiles_suggestion, null);

        View tilesView = getTileCoordinator().getView();
        if (tilesView.getParent() != suggestionView) UiUtils.removeViewFromParent(tilesView);
        suggestionView.addView(tilesView);
        return suggestionView;
    }

    /** @return A {@link ImageTileCoordinator} instance. Creates if it doesn't exist yet. */
    private ImageTileCoordinator getTileCoordinator() {
        if (mTileCoordinator == null) {
            TileConfig tileConfig = new TileConfig.Builder().setUmaPrefix(UMA_PREFIX).build();
            mTileCoordinator = ImageTileCoordinatorFactory.create(
                    mContext, tileConfig, this::onTileClicked, this::getVisuals);
        }
        return mTileCoordinator;
    }

    /**
     * Method called by the query tiles widget to fetch the bitmap to be shown for a given tile.
     * @param tile The associated query tile.
     * @param callback The callback to be invoked by the backend when bitmap is available.
     */
    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
        if (mTileWidth == null) {
            mTileWidth = mContext.getResources().getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.tile_ideal_width);
        }

        QueryTile queryTile = (QueryTile) tile;
        getImageFetcher().fetchImage(queryTile.urls.get(0), ImageFetcher.QUERY_TILE_UMA_CLIENT_NAME,
                mTileWidth, mTileWidth, bitmap -> callback.onResult(Arrays.asList(bitmap)));
    }

    /** Create an ImageFetcher instance. Only created if needed. */
    private ImageFetcher getImageFetcher() {
        if (mImageFetcher == null) {
            mImageFetcher = ImageFetcherFactory.createImageFetcher(
                    ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                    GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);
        }
        return mImageFetcher;
    }

    private void onTileClicked(ImageTile tile) {
        QueryTile queryTile = (QueryTile) tile;
        mSelectionCallback.onResult(queryTile);
    }
}

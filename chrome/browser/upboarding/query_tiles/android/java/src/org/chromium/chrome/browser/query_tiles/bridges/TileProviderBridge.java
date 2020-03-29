// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles.bridges;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.query_tiles.Tile;
import org.chromium.chrome.browser.query_tiles.TileProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to the native query tile service for the given {@link Profile}.
 */
@JNINamespace("upboarding")
public class TileProviderBridge implements TileProvider {
    private long mNativeTileProviderBridge;

    @CalledByNative
    private static TileProviderBridge create(long nativePtr) {
        return new TileProviderBridge(nativePtr);
    }

    private TileProviderBridge(long nativePtr) {
        mNativeTileProviderBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTileProviderBridge = 0;
    }

    @Override
    public void getQueryTiles(Callback<List<Tile>> callback) {
        if (mNativeTileProviderBridge == 0) return;
        TileProviderBridgeJni.get().getQueryTiles(mNativeTileProviderBridge, this, callback);
    }

    @Override
    public void getVisuals(String id, Callback<List<Bitmap>> callback) {
        if (mNativeTileProviderBridge == 0) return;
        TileProviderBridgeJni.get().getVisuals(mNativeTileProviderBridge, this, id, callback);
    }

    @CalledByNative
    private static List<Tile> createList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static Tile createTileAndMaybeAddToList(@Nullable List<Tile> list, String tileId,
            String displayTitle, String accessibilityText, String queryText, List<Tile> children) {
        Tile tile = new Tile(tileId, displayTitle, accessibilityText, queryText, children);
        if (list != null) list.add(tile);
        return tile;
    }

    @NativeMethods
    interface Natives {
        void getQueryTiles(long nativeTileProviderBridge, TileProviderBridge caller,
                Callback<List<Tile>> callback);
        void getVisuals(long nativeTileProviderBridge, TileProviderBridge caller, String id,
                Callback<List<Bitmap>> callback);
    }
}
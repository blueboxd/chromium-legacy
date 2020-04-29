// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.query_tiles.TileProvider;

/**
 * Basic factory that creates and returns an {@link TileProvider} that is attached
 * natively to the given {@link Profile}.
 */
public class TileProviderFactory {
    private static TileProvider sTileProvider;

    /**
     * Used to get access to the tile provider backend.
     * @return An {@link TileProvider} instance.
     */
    public static TileProvider getForProfile(Profile profile) {
        if (sTileProvider == null) {
            sTileProvider = TileProviderFactoryJni.get().getForProfile(profile);
        }

        return sTileProvider;
    }

    /** For testing only. */
    public static void setTileProviderForTesting(TileProvider provider) {
        sTileProvider = provider;
    }

    // TODO(shaktisahu): Remove this function once we have the real provider.
    public static void setFakeTileProvider(FakeTileProvider provider) {
        if (sTileProvider != null) return;
        sTileProvider = provider;
    }

    @NativeMethods
    interface Natives {
        TileProvider getForProfile(Profile profile);
    }
}

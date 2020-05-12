// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Handles various feature utility functions for query tiles.
 */
public class QueryTileUtils {
    /**
     * This is the main feature flag for query tiles. All other flags are effective only when this
     * flag is enabled.
     * @return Whether the query tile feature is enabled.
     */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES);
    }

    /**
     * This is one experimental variation where user will have a chance of editing the query text
     * before starting a search. When a query tile is clicked, the query text will be pasted in the
     * omnibox. No second level tiles will be displayed. This is meant to show only one level of
     * query tiles.
     * @return Whether the user should have a chance to edit the query text before starting a
     *         search.
     */
    public static boolean isQueryEditingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_ENABLE_QUERY_EDITING);
    }
}

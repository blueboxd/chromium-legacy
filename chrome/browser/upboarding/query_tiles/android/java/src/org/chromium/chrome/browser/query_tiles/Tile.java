// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Class encapsulating data needed to render a query tile for the query sites section on the NTP.
 */
public class Tile {
    /** The ID representing this tile. */
    public final String id;

    /** The text to be shown on this tile. */
    public final String displayTitle;

    /** The text to be shown in accessibility mode. */
    public final String accessibilityText;

    /** The string to be used for building a query when this tile is clicked. */
    public final String queryText;

    /** The next level tiles to be shown when this tile is clicked. */
    public final List<Tile> children;

    /** Constructor. */
    public Tile(String id, String displayTitle, String accessibilityText, String queryText,
            List<Tile> children) {
        this.id = id;
        this.displayTitle = displayTitle;
        this.accessibilityText = accessibilityText;
        this.queryText = queryText;
        this.children =
                Collections.unmodifiableList(children == null ? new ArrayList<>() : children);
    }
}

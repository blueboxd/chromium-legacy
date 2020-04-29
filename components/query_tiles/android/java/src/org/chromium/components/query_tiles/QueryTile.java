// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Class encapsulating data needed to render a query tile for the query sites section on the NTP.
 */
public class QueryTile extends ImageTile {
    /** The string to be used for building a query when this tile is clicked. */
    public final String queryText;

    /** The next level tiles to be shown when this tile is clicked. */
    public final List<QueryTile> children;

    /** Constructor. */
    public QueryTile(String id, String displayTitle, String accessibilityText, String queryText,
            List<QueryTile> children) {
        super(id, displayTitle, accessibilityText);
        this.queryText = queryText;
        this.children =
                Collections.unmodifiableList(children == null ? new ArrayList<>() : children);
    }
}

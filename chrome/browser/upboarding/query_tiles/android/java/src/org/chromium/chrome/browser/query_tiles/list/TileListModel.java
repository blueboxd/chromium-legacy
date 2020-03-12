// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles.list;

import org.chromium.chrome.browser.query_tiles.Tile;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This model represents the data required to build a list UI around a set of {@link Tile}s.
 * This includes (1) a {@link ListModel} implementation and (2) exposing a
 * {@link PropertyModel} for shared item properties and general list information.
 */
class TileListModel extends ListModel<Tile> {
    private final PropertyModel mListProperties = new PropertyModel(TileListProperties.ALL_KEYS);

    /**
     * @return A {@link PropertyModel} instance, which is a set of shared properties for the
     *         list.
     */
    public PropertyModel getProperties() {
        return mListProperties;
    }
}

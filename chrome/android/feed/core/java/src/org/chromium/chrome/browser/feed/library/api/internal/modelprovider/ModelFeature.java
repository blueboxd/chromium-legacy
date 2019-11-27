// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import org.chromium.chrome.browser.feed.library.common.feedobservable.Observable;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;

/**
 * This represents a Feature model within the stream tree structure. This represents things such as
 * a Cluster, Carousel, Piet Content, etc. Features provide an {@link ModelCursor} for accessing the
 * children of the feature.
 */
public interface ModelFeature extends Observable<FeatureChangeObserver> {
    /**
     * Returns the {@link StreamFeature} proto instance allowing for access of the metadata and
     * payload.
     */
    StreamFeature getStreamFeature();

    /**
     * An Cursor over the children of the feature. This Cursor is a one way iterator over the
     * children. If the feature does not contain children, an empty cursor will be returned.
     *
     * <p>Each call to this method will return a new instance of the ModelCursor.
     */
    ModelCursor getCursor();

    /**
     * Create a ModelCursor which advances in the defined direction (forward or reverse), it may
     * also start at a specific child. If the specified child is not found, this will return {@code
     * null}. If {@code startingChild} is {@code null}, the cursor starts at the start (beginning or
     * end) of the child list.
     */
    /*@Nullable*/
    ModelCursor getDirectionalCursor(boolean forward, /*@Nullable*/ String startingChild);
}

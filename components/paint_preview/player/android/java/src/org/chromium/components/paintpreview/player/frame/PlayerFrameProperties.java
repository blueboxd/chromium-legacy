// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.Pair;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains all properties that a player frame {@link PropertyModel} can have.
 */
class PlayerFrameProperties {
    /** A matrix of bitmap tiles that collectively make the entire content. */
    static final PropertyModel.WritableObjectPropertyKey<Bitmap[][]> BITMAP_MATRIX =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    /**
     * Contains the current user-visible content window. The view should use this to draw the
     * appropriate bitmap tiles from {@link #BITMAP_MATRIX}.
     */
    static final PropertyModel.WritableObjectPropertyKey<Rect> VIEWPORT =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    /**
     * A list of sub-frames that are currently visible. Each element in the list is a {@link Pair}
     * consists of a {@link View}, that displays the sub-frame's content, and a {@link Rect}, that
     * contains its location.
     */
    static final PropertyModel.WritableObjectPropertyKey<List<Pair<View, Rect>>> SUBFRAME_VIEWS =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    static final PropertyKey[] ALL_KEYS = {BITMAP_MATRIX, VIEWPORT, SUBFRAME_VIEWS};
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import androidx.annotation.Nullable;

/**
 * This represents a Cursor through the children of a {@link ModelFeature}. A Cursor will provide
 * forward only access to the children of the container. When a cursor is created by calling {@link
 * ModelFeature#getCursor()}, it is positioned at the first child.
 */
public interface ModelCursor {
    /** Returns the next {@link ModelChild} in the cursor or {@code null} if at end. */
    @Nullable
    ModelChild getNextItem();

    /** Returns {@literal true} if the cursor is at the end. */
    boolean isAtEnd();
}

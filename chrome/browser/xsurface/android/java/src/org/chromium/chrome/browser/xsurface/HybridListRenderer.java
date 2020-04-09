// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import androidx.annotation.Nullable;

/**
 * A renderer that can handle mixing externally-provided views with native Android views
 * in a RecyclerView.
 */
public interface HybridListRenderer {
    /**
     * Binds a contentmanager with this renderer.
     * @return a View that the HybridListRenderer is managing, which can then be
     * attached to other view
     */
    @Nullable
    default View bind(ListContentManager manager) {
        return null;
    }

    /**
     * Unbinds a previously attached recyclerview and contentmanager.
     *
     * Does nothing if nothing was previously bound.
     */
    default void unbind() {}

    /**
     * Updates the renderer with templates and initializing data.
     */
    default void update(byte[] data) {}
}

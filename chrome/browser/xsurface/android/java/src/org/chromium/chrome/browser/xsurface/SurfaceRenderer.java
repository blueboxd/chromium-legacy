// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import java.util.Map;

/**
 * Interface to call a rendering service to render a View sent by a server.
 */
public interface SurfaceRenderer {
    /** Update the card renderer with shared data bytes. */
    void update(byte[] data);

    /**
     * Turns a stream of externally-provided bytes into an Android View.
     *
     * @param renderData externally-provided bytes to be rendered.
     * @param contextValues additional context to be incorporated into the view.
     */
    View render(byte[] renderData, Map<String, Object> contextValues);
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

/**
 * Class encapsulating data needed to render a image tile. An {@link ImageTile} is a tile meant to
 * show an image with some text.
 */
public class ImageTile {
    /** The ID representing this tile. */
    public final String id;

    /** The text to be shown on this tile. */
    public final String displayTitle;

    /** The text to be shown in accessibility mode. */
    public final String accessibilityText;

    /** Constructor. */
    public ImageTile(String id, String displayTitle, String accessibilityText) {
        this.id = id;
        this.displayTitle = displayTitle;
        this.accessibilityText = accessibilityText;
    }
}

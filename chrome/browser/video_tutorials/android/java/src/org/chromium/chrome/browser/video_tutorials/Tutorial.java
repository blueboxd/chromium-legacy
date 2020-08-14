// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

/**
 * Class encapsulating data needed to show a video tutorial on the UI.
 */
public class Tutorial extends ImageTile {
    // TODO(shaktisahu): Add video tutorial specific fields.

    /** Constructor */
    public Tutorial(String id, String displayTitle, String accessibilityText) {
        super(id, displayTitle, accessibilityText);
    }
}

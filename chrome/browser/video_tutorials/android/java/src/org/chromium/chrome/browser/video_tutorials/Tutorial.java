// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

/**
 * Class encapsulating data needed to show a video tutorial on the UI.
 */
public class Tutorial extends ImageTile {
    public final @FeatureType int featureType;
    public final String videoUrl;
    public final String posterUrl;
    public final String captionUrl;
    public final String shareUrl;
    public final int videoLength;

    /** Constructor */
    public Tutorial(@FeatureType int featureType, String title, String videoUrl, String posterUrl,
            String captionUrl, String shareUrl, int videoLength) {
        super(createIdFromFeatureType(featureType), title, title);
        this.featureType = featureType;
        this.videoUrl = videoUrl;
        this.posterUrl = posterUrl;
        this.captionUrl = captionUrl;
        this.shareUrl = shareUrl;
        this.videoLength = videoLength;
    }

    private static String createIdFromFeatureType(int featureType) {
        return String.valueOf(featureType);
    }
}

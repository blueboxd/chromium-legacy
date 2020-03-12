// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.imageloader;

import androidx.annotation.StringDef;

/** Enumerates the set of bundled assets which could be requested via the {@link ImageLoaderApi}. */
@StringDef({BundledAssets.OFFLINE_INDICATOR_BADGE, BundledAssets.OFFLINE_INDICATOR_BADGE_DARK_BG,
        BundledAssets.VIDEO_INDICATOR_BADGE, BundledAssets.VIDEO_INDICATOR_BADGE_DARK_BG,
        BundledAssets.MENU_ICON, BundledAssets.MENU_ICON_DARK_BG, BundledAssets.AMP_ICON,
        BundledAssets.AMP_ICON_DARK_BG, BundledAssets.SPARK_ICON, BundledAssets.SPARK_ICON_DARK_BG})
public @interface BundledAssets {
    /** Icon that indicates amp content */
    String AMP_ICON = "amp_icon";

    /** Dark Theme Background Icon that indicates amp content */
    String AMP_ICON_DARK_BG = "amp_icon_dark";

    /** Icon that indicates a click will take the user to a menu. */
    String MENU_ICON = "menu_icon";

    /** Dark Theme Background Icon that indicates a click will take the user to a menu. */
    String MENU_ICON_DARK_BG = "menu_icon_dark";

    /** Badge to show indicating content is available offline. */
    String OFFLINE_INDICATOR_BADGE = "offline_indicator_badge";

    /** Dark Theme Background Badge to show indicating content is available offline. */
    String OFFLINE_INDICATOR_BADGE_DARK_BG = "offline_indicator_badge_dark";

    /** Branding interest spark icon. */
    String SPARK_ICON = "spark_icon";

    /** Dark Theme Background Branding interest spark icon. */
    String SPARK_ICON_DARK_BG = "spark_icon_dark";

    /** Badge to show indicating content links to a video. */
    String VIDEO_INDICATOR_BADGE = "video_indicator_badge";

    /** Dark Theme Background Badge to show indicating content links to a video. */
    String VIDEO_INDICATOR_BADGE_DARK_BG = "video_indicator_badge_dark";
}

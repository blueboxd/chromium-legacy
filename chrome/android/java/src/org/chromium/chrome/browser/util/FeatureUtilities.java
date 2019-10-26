// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

/**
 * A bridge class to {@link org.chromium.chrome.browser.flags.FeatureUtilities}.
 * TODO(crbug.com/995916): Remove this when all references are gone.
 */
public class FeatureUtilities {

    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        return org.chromium.chrome.browser.flags.FeatureUtilities.isBottomToolbarEnabled();
    }

    /**
     * @return Whether immersive ui mode is enabled.
     */
    public static boolean isImmersiveUiModeEnabled() {
        return org.chromium.chrome.browser.flags.FeatureUtilities.isImmersiveUiModeEnabled();
    }
}

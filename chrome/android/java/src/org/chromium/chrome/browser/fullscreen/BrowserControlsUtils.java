// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

/**
 * Static utilities related to browser controls interfaces.
 */
public class BrowserControlsUtils {
    /**
     * @return True if the browser controls are completely off screen.
     */
    public static boolean areBrowserControlsOffScreen(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() == 1.0f;
    }

    /**
     * @return True if the browser controls are currently completely visible.
     */
    public static boolean areBrowserControlsFullyVisible(
            BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() == 0.f;
    }

    /**
     * @return Whether the browser controls should be drawn as a texture.
     */
    public static boolean drawControlsAsTexture(BrowserControlsStateProvider stateProvider) {
        return stateProvider.getBrowserControlHiddenRatio() > 0;
    }
}

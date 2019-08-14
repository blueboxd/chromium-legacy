// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import org.chromium.chrome.browser.widget.PulseDrawable;

/**
 * Allows for testing of views which are highlightable via ViewHighlighter.
 */
public class ViewHighlighterTestUtils {
    /**
     * Returns true if the provided view is currently being highlighted.
     * Please note that this function may not be the same as !checkHighlightOff.
     *
     * @param view The view which you'd like to check for highlighting.
     * @return True if the view is currently being highlighted.
     */
    public static boolean checkHighlightOn(View view) {
        if (!(view.getBackground() instanceof LayerDrawable)) return false;
        LayerDrawable layerDrawable = (LayerDrawable) view.getBackground();
        Drawable drawable = layerDrawable.getDrawable(layerDrawable.getNumberOfLayers() - 1);
        if (!(drawable instanceof PulseDrawable)) return false;
        PulseDrawable pulse = (PulseDrawable) drawable;
        return pulse.isRunning() && pulse.isVisible();
    }

    /**
     * Returns true if the provided view is not currently being highlighted.
     * Please note that this function may not be the same as !checkHighlightOn.
     *
     * @param view The view which you'd like to check for highlighting.
     * @return True if view is not currently being highlighted.
     */
    public static boolean checkHighlightOff(View view) {
        return !(view.getBackground() instanceof LayerDrawable);
    }
}
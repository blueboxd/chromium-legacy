// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RectShape;

import org.chromium.base.Supplier;
import org.chromium.components.feed.core.proto.ui.piet.GradientsProto.LinearGradient;

/** Class to display gradients according to the Piet spec (angle + color stops at %ages) */
public class GradientDrawable extends ShapeDrawable {
    // Doesn't like the call to setShape and setShaderFactory
    @SuppressWarnings("initialization")
    public GradientDrawable(LinearGradient gradient, Supplier<Boolean> rtlSupplier) {
        super();

        int[] colors = new int[gradient.getStopsCount()];
        float[] stops = new float[gradient.getStopsCount()];
        for (int i = 0; i < gradient.getStopsCount(); i++) {
            colors[i] = gradient.getStops(i).getColor();
            stops[i] = gradient.getStops(i).getPositionPercent() / 100.0f;
        }
        setShape(new RectShape());
        setShaderFactory(new GradientShader(colors, stops, gradient.getDirectionDeg(),
                gradient.getReverseForRtl() ? rtlSupplier : null));
    }
}

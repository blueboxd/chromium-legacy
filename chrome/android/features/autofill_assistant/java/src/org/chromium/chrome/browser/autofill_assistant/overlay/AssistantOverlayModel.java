// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.Color;
import android.graphics.RectF;
import android.support.annotation.ColorInt;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantOverlayModel extends PropertyModel {
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<List<RectF>> TOUCHABLE_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<RectF>> RESTRICTED_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<RectF> VISUAL_VIEWPORT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantOverlayDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> BACKGROUND_COLOR =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> HIGHLIGHT_BORDER_COLOR =
            new WritableObjectPropertyKey<>();

    public AssistantOverlayModel() {
        super(STATE, TOUCHABLE_AREA, RESTRICTED_AREA, VISUAL_VIEWPORT, DELEGATE, BACKGROUND_COLOR,
                HIGHLIGHT_BORDER_COLOR);
    }

    @CalledByNative
    private void setState(@AssistantOverlayState int state) {
        set(STATE, state);
    }

    @CalledByNative
    private void setVisualViewport(float left, float top, float right, float bottom) {
        set(VISUAL_VIEWPORT, new RectF(left, top, right, bottom));
    }

    @CalledByNative
    private void setTouchableArea(float[] coords) {
        set(TOUCHABLE_AREA, toRectangles(coords));
    }

    private static List<RectF> toRectangles(float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        return boxes;
    }

    @CalledByNative
    private void setRestrictedArea(float[] coords) {
        set(RESTRICTED_AREA, toRectangles(coords));
    }

    @CalledByNative
    private void setDelegate(AssistantOverlayDelegate delegate) {
        set(DELEGATE, delegate);
    }

    @CalledByNative
    private boolean setBackgroundColor(String colorString) {
        return setColor(BACKGROUND_COLOR, colorString);
    }

    @CalledByNative
    private boolean setHighlightBorderColor(String colorString) {
        return setColor(HIGHLIGHT_BORDER_COLOR, colorString);
    }

    /**
     * Sets the given color property.
     *
     * @param property property to set
     * @param colorString color value as a property. The empty string means use the default color
     * @return true if the color string was parsed and set properly
     */
    private boolean setColor(WritableObjectPropertyKey<Integer> property, String colorString) {
        if (colorString.isEmpty()) {
            set(property, null);
            return true;
        }
        @ColorInt
        int colorInt;
        try {
            set(property, Color.parseColor(colorString));
            return true;
        } catch (IllegalArgumentException e) {
            set(property, null);
            return false;
        }
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A means of showing highlight for a particular feature as a form of in product help. */
public class FeatureHighlightProvider {
    /**
     * These values determine text alignment and need to match the values in the closed-source
     * library.
     */
    @IntDef({TextAlignment.START, TextAlignment.CENTER, TextAlignment.END})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TextAlignment {
        int START = 0;
        int CENTER = 1;
        int END = 2;
    }

    /** The value for IPH timeout if the IPH will not timeout. */
    public static final long NO_TIMEOUT = -1;

    public FeatureHighlightProvider() {}

    /**
     * Build and show a feature highlight bubble for a particular view.
     * @param activity An activity to attach the highlight to.
     * @param view The view to focus.
     * @param headTextId The text shown in the header section of the bubble.
     * @param headAlignment Alignment of the head text.
     * @param headStyle Style of the head text size and color.
     * @param bodyTextId The text shown in the body section of the bubble.
     * @param bodyAlignment Alignment of the body text.
     * @param bodyStyle Style of the body text size and color.
     * @param pulseColor The inner color of the bubble.
     * @param outerColor The outer color of the bubble.
     * @param scrimColor The color of the out side of feature highlight.
     * @param timeoutMs The amount of time in ms before the bubble disappears.
     * @param completeRunnable The Runnable to be called if the user tab on the view.
     */
    public void buildForView(AppCompatActivity activity, View view, @StringRes int headTextId,
            @TextAlignment int headAlignment, @StyleRes int headStyle, @StringRes int bodyTextId,
            @TextAlignment int bodyAlignment, @StyleRes int bodyStyle, @ColorInt int pulseColor,
            @ColorInt int outerColor, @ColorInt int scrimColor, long timeoutMs,
            Runnable completeRunnable) {}

    /**
     * Build and show a feature highlight bubble for a particular view.
     * @param activity An activity to attach the highlight to.
     * @param view The view to focus.
     * @param headTextId The text shown in the header section of the bubble.
     * @param headAlignment Alignment of the head text.
     * @param headStyle Style of the head text size and color.
     * @param bodyTextId The text shown in the body section of the bubble.
     * @param bodyAlignment Alignment of the body text.
     * @param bodyStyle Style of the body text size and color.
     * @param pulseColor The inner color of the bubble.
     * @param outerColor The outer color of the bubble.
     * @param scrimColor The color of the out side of feature highlight.
     * @param timeoutMs The amount of time in ms before the bubble disappears.
     * @param tapToDismiss The feature highlight bubble can be dismissiable.
     * @param completeRunnable The Runnable to be called if the user tab on the view.
     */
    public void buildForView(AppCompatActivity activity, View view, @StringRes int headTextId,
            @TextAlignment int headAlignment, @StyleRes int headStyle, @StringRes int bodyTextId,
            @TextAlignment int bodyAlignment, @StyleRes int bodyStyle, @ColorInt int pulseColor,
            @ColorInt int outerColor, @ColorInt int scrimColor, long timeoutMs,
            Boolean tapToDismiss, Runnable completeRunnable) {}

    /**
     * Dismiss the feature highlight bubble for a particular view.
     * @param activity An activity to attach the IPH to.
     */
    public void dismiss(AppCompatActivity activity) {}
}

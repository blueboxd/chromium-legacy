// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.widget.ChromeImageView;

/** Generic view factory. */
@JNINamespace("autofill_assistant")
public class AssistantViewFactory {
    /** Attaches {@code view} to {@code container}. */
    @CalledByNative
    public static void addViewToContainer(ViewGroup container, View view) {
        container.addView(view);
    }

    /** Set view attributes. All padding values are in dp. */
    @CalledByNative
    public static void setViewAttributes(View view, Context context, int paddingStart,
            int paddingTop, int paddingEnd, int paddingBottom,
            @Nullable AssistantDrawable background) {
        view.setPaddingRelative(AssistantDimension.getPixelSizeDp(context, paddingStart),
                AssistantDimension.getPixelSizeDp(context, paddingTop),
                AssistantDimension.getPixelSizeDp(context, paddingEnd),
                AssistantDimension.getPixelSizeDp(context, paddingBottom));
        if (background != null) {
            background.getDrawable(context, result -> {
                if (result != null) {
                    view.setBackground(result);
                }
            });
        }
    }

    /**
     * Sets layout parameters for {@code view}. {@code width} and {@code height} must bei either
     * MATCH_PARENT (-1), WRAP_CONTENT (-2) or a value in dp.
     */
    @CalledByNative
    public static void setViewLayoutParams(View view, Context context, int width, int height,
            float weight, int marginStart, int marginTop, int marginEnd, int marginBottom,
            int layoutGravity) {
        if (width > 0) {
            width = AssistantDimension.getPixelSizeDp(context, width);
        }
        if (height > 0) {
            height = AssistantDimension.getPixelSizeDp(context, height);
        }

        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(width, height);
        layoutParams.weight = weight;
        layoutParams.setMarginStart(AssistantDimension.getPixelSizeDp(context, marginStart));
        layoutParams.setMarginEnd(AssistantDimension.getPixelSizeDp(context, marginEnd));
        layoutParams.topMargin = AssistantDimension.getPixelSizeDp(context, marginTop);
        layoutParams.bottomMargin = AssistantDimension.getPixelSizeDp(context, marginBottom);
        layoutParams.gravity = layoutGravity;
        view.setLayoutParams(layoutParams);
    }

    /** Creates a {@code android.widget.LinearLayout} widget. */
    @CalledByNative
    public static LinearLayout createLinearLayout(
            Context context, String identifier, int orientation) {
        LinearLayout linearLayout = new LinearLayout(context);
        linearLayout.setOrientation(orientation);
        linearLayout.setTag(identifier);
        return linearLayout;
    }

    /** Creates a {@code android.widget.TextView} widget. */
    @CalledByNative
    public static TextView createTextView(
            Context context, String identifier, String text, @Nullable String textAppearance) {
        TextView textView = new TextView(context);
        textView.setTag(identifier);
        textView.setText(text);
        if (textAppearance != null) {
            int styleId = context.getResources().getIdentifier(
                    textAppearance, "style", context.getPackageName());
            if (styleId != 0) {
                ApiCompatibilityUtils.setTextAppearance(textView, styleId);
            }
        }
        return textView;
    }

    /** Creates a divider widget as used in the {@code AssistantCollectUserData} action. */
    @CalledByNative
    public static View createDividerView(Context context, String identifier) {
        View divider = LayoutInflater.from(context).inflate(
                org.chromium.chrome.autofill_assistant.R.layout
                        .autofill_assistant_payment_request_section_divider,
                null, false);
        divider.setTag(identifier);
        return divider;
    }

    /** Creates a {@code ChromeImageView} widget. */
    @CalledByNative
    public static ChromeImageView createImageView(
            Context context, String identifier, AssistantDrawable image) {
        ChromeImageView imageView = new ChromeImageView(context);
        imageView.setTag(identifier);
        image.getDrawable(context, result -> {
            if (result != null) {
                imageView.setImageDrawable(result);
            }
        });
        return imageView;
    }
}

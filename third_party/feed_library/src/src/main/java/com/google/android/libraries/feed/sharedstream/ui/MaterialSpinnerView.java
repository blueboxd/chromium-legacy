// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.ui;

import android.content.Context;
import android.content.res.Resources.Theme;
import android.support.v4.widget.CircularProgressDrawable;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;

/** View compatible with KitKat that shows a Material themed spinner. */
public class MaterialSpinnerView extends AppCompatImageView {
    private final CircularProgressDrawable spinner;

    public MaterialSpinnerView(Context context) {
        this(context, null);
    }

    public MaterialSpinnerView(Context context, /*@Nullable*/ AttributeSet attrs) {
        this(context, attrs, 0);
    }

    // In android everything is initialized properly after the constructor call, so it is fine to do
    // this work after.
    @SuppressWarnings({"nullness:argument.type.incompatible", "nullness:method.invocation.invalid"})
    public MaterialSpinnerView(Context context, /*@Nullable*/ AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        spinner = new CircularProgressDrawable(context);
        spinner.setStyle(CircularProgressDrawable.DEFAULT);
        setImageDrawable(spinner);
        TypedValue typedValue = new TypedValue();
        Theme theme = context.getTheme();
        theme.resolveAttribute(R.attr.feedSpinnerColor, typedValue, true);
        spinner.setColorSchemeColors(typedValue.data);

        if (getVisibility() == View.VISIBLE) {
            spinner.start();
        }
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);

        if (spinner.isRunning() && getVisibility() != View.VISIBLE) {
            spinner.stop();
        } else if (!spinner.isRunning() && getVisibility() == View.VISIBLE) {
            spinner.start();
        }
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import static org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantViewFactory.setViewLayoutParams;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.Gravity;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.RadioButton;

import org.chromium.base.Callback;

/**
 * Custom toggle button that supports arbitrary content views.
 */
class AssistantToggleButton extends LinearLayout {
    final CompoundButton mToggleButton;

    AssistantToggleButton(Context context, Callback<Boolean> onCheckedChanged,
            @Nullable View leftContentView, @Nullable View rightContentView, boolean isCheckbox) {
        super(context, null);
        CompoundButton toggle;
        if (isCheckbox) {
            mToggleButton = new CheckBox(context, null);
        } else {
            mToggleButton = new RadioButton(context, null);
        }

        setOrientation(LinearLayout.HORIZONTAL);
        if (leftContentView != null) {
            addView(leftContentView);
        }
        setViewLayoutParams(mToggleButton, context, LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT, /* weight = */ 0.0f, /* marginStart = */ 0,
                /* marginTop = */ 0, /* marginEnd = */ 0, /* marginBottom = */ 0,
                Gravity.CENTER_VERTICAL, /* minimumWidth = */ 0, /* minimumHeight = */ 0);
        addView(mToggleButton);
        if (rightContentView != null) {
            addView(rightContentView);
        }

        setOnClickListener(unusedView
                -> mToggleButton.setChecked(isCheckbox ? !mToggleButton.isChecked() : true));
        mToggleButton.setOnCheckedChangeListener(
                (unusedView, isChecked) -> onCheckedChanged.onResult(isChecked));
    }

    void setChecked(boolean checked) {
        mToggleButton.setChecked(checked);
    }
}

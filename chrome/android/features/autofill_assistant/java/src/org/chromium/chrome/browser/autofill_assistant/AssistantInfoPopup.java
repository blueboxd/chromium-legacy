// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.UiUtils;

/**
 * Represents a simple info popup.
 */
@JNINamespace("autofill_assistant")
public class AssistantInfoPopup {
    private final String mTitle;
    private final String mText;
    @Nullable
    private final AssistantDialogButton mPositiveButton;
    @Nullable
    private final AssistantDialogButton mNegativeButton;
    @Nullable
    private final AssistantDialogButton mNeutralButton;

    @CalledByNative
    public AssistantInfoPopup(String title, String text,
            @Nullable AssistantDialogButton positiveButton,
            @Nullable AssistantDialogButton negativeButton,
            @Nullable AssistantDialogButton neutralButton) {
        mTitle = title;
        mText = text;
        mPositiveButton = positiveButton;
        mNegativeButton = negativeButton;
        mNeutralButton = neutralButton;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getText() {
        return mText;
    }

    public void show(Context context) {
        AlertDialog.Builder builder = new UiUtils
                                              .CompatibleAlertDialogBuilder(context,
                                                      org.chromium.chrome.autofill_assistant.R.style
                                                              .Theme_Chromium_AlertDialog)
                                              .setTitle(mTitle)
                                              .setMessage(mText);

        if (mPositiveButton != null) {
            builder.setPositiveButton(mPositiveButton.getLabel(),
                    (dialog, which) -> mPositiveButton.onClick(context));
        }
        if (mNegativeButton != null) {
            builder.setNegativeButton(mNegativeButton.getLabel(),
                    (dialog, which) -> mNegativeButton.onClick(context));
        }
        if (mNeutralButton != null) {
            builder.setNeutralButton(
                    mNeutralButton.getLabel(), (dialog, which) -> mNeutralButton.onClick(context));
        }
        builder.show();
    }
}
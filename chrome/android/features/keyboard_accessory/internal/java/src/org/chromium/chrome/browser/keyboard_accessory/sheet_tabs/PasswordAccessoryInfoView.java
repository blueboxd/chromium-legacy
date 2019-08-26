// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.ui.widget.ChipView;

/**
 * This view represents a section of user credentials in the password tab of the keyboard accessory.
 */
class PasswordAccessoryInfoView extends LinearLayout {
    private TextView mTitle;
    private ImageView mIcon;
    private ChipView mUsername;
    private ChipView mPassword;

    /**
     * Constructor for inflating from XML.
     */
    public PasswordAccessoryInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.password_info_title);
        mIcon = findViewById(R.id.favicon);
        mUsername = findViewById(R.id.suggestion_text);
        mPassword = findViewById(R.id.password_text);
    }

    void setIconForBitmap(@Nullable Bitmap favicon) {
        Drawable icon;
        if (favicon == null) {
            icon = AppCompatResources.getDrawable(getContext(), R.drawable.ic_globe_36dp);
        } else {
            icon = new BitmapDrawable(getContext().getResources(), favicon);
        }
        final int kIconSize = getContext().getResources().getDimensionPixelSize(
                R.dimen.keyboard_accessory_suggestion_icon_size);
        icon.setBounds(0, 0, kIconSize, kIconSize);
        mIcon.setImageDrawable(icon);
    }

    TextView getTitle() {
        return mTitle;
    }

    ChipView getUsername() {
        return mUsername;
    }

    ChipView getPassword() {
        return mPassword;
    }
}

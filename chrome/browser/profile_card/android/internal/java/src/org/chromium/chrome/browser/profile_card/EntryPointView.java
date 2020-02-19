// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.profile_card.internal.R;

/**
 * UI component that handles showing a profile card view.
 */
public class EntryPointView extends LinearLayout {
    private View mMainContentView;
    private ImageButton mImageButton;

    public EntryPointView(Context context) {
        super(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMainContentView =
                LayoutInflater.from(getContext()).inflate(R.layout.entry_point, /*root=*/null);
        mImageButton = mMainContentView.findViewById(R.id.entry_point_icon);
    }

    void setIconBitmap(Bitmap iconBitmap) {
        mImageButton.setImageBitmap(iconBitmap);
    }

    void setVisibility(boolean visible) {
        if (visible) {
            mMainContentView.setVisibility(View.VISIBLE);
        } else {
            mMainContentView.setVisibility(View.GONE);
        }
    }
}

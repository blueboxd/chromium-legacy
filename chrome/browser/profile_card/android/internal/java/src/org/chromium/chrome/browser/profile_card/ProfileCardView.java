// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.method.ScrollingMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.profile_card.internal.R;

/**
 * UI component that handles showing a text bubble with a preceding image.
 */
public class ProfileCardView extends LinearLayout {
    private View mMainContentView;
    private TextView mTitleTextView;
    private TextView mDescriptionTextView;
    private ImageView mAvatarView;
    private TextView mPostFrequencyTextView;

    public ProfileCardView(Context context) {
        super(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMainContentView =
                LayoutInflater.from(getContext()).inflate(R.layout.profile_card, /*root=*/null);
        mTitleTextView = mMainContentView.findViewById(R.id.title);
        mDescriptionTextView = mMainContentView.findViewById(R.id.description);
        mDescriptionTextView.setMovementMethod(new ScrollingMovementMethod());
        mPostFrequencyTextView = mMainContentView.findViewById(R.id.post_freq);
    }

    void setAvatarBitmap(Bitmap avatarBitmap) {
        mAvatarView.setImageBitmap(avatarBitmap);
    }

    void setTitle(String title) {
        if (mTitleTextView == null) {
            throw new IllegalStateException("Current dialog doesn't have a title text view");
        }
        mTitleTextView.setText(title);
    }

    void setDescription(String description) {
        if (mDescriptionTextView == null) {
            throw new IllegalStateException("Current dialog doesn't have a description text view");
        }
        mDescriptionTextView.setText(description);
    }

    void setPostFrequency(String postFrequency) {
        if (mPostFrequencyTextView == null) {
            throw new IllegalStateException(
                    "Current dialog doesn't have a post frequency text view");
        }
        mPostFrequencyTextView.setText(postFrequency);
    }

    void setVisibility(boolean visible) {
        if (visible) {
            mMainContentView.setVisibility(View.VISIBLE);
        } else {
            mMainContentView.setVisibility(View.GONE);
        }
    }
}

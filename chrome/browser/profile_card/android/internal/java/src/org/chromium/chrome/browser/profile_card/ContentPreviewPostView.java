// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.profile_card.internal.R;

/**
 * UI component that handles a single content preview post.
 */
public class ContentPreviewPostView extends FrameLayout {
    private View mMainContentView;
    private TextView mTitleTextView;
    private TextView mPostedTimeTextView;
    private ImageView mPostImageView;

    public ContentPreviewPostView(Context context) {
        super(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMainContentView = LayoutInflater.from(getContext())
                                   .inflate(R.layout.content_preview_post, /*root=*/null);
        mTitleTextView = mMainContentView.findViewById(R.id.title);
        mPostedTimeTextView = mMainContentView.findViewById(R.id.posted_time);
        mPostImageView = mMainContentView.findViewById(R.id.post_image);
    }

    void setImageBitmap(Bitmap imageBitmap) {
        mPostImageView.setImageBitmap(imageBitmap);
        mPostImageView.setVisibility(View.VISIBLE);
    }

    void setTitle(String title) {
        if (mTitleTextView == null) {
            throw new IllegalStateException("Current post doesn't have a title text view");
        }
        mTitleTextView.setText(title);
    }

    void setPostedTime(String postedTime) {
        if (mPostedTimeTextView == null) {
            throw new IllegalStateException("Current post doesn't have a posted time text view");
        }
        mPostedTimeTextView.setText(postedTime);
    }

    void setVisibility(boolean visible) {
        if (visible) {
            mMainContentView.setVisibility(View.VISIBLE);
        } else {
            mMainContentView.setVisibility(View.GONE);
        }
    }
}

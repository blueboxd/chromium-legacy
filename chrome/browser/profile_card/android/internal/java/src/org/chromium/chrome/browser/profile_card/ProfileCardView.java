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
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;

/**
 * UI component that handles showing a profile card view.
 */
public class ProfileCardView extends TextBubble {
    private View mMainContentView;
    private TextView mTitleTextView;
    private TextView mDescriptionTextView;
    private ImageView mAvatarView;
    private TextView mPostFrequencyTextView;
    private LinearLayout mPostsContainer;

    /**
     * Constructs a {@link ProfileCardView} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     */
    public ProfileCardView(Context context, View rootView, String stringId,
            String accessibilityStringId, RectProvider anchorRectProvider) {
        super(context, rootView, stringId, accessibilityStringId, /*showArrow=*/true,
                anchorRectProvider, /*isAccessibilityEnabled=*/true);
        setDismissOnTouchInteraction(true);
    }

    @Override
    protected View createContentView() {
        mMainContentView =
                LayoutInflater.from(mContext).inflate(R.layout.profile_card, /*root=*/null);
        mTitleTextView = mMainContentView.findViewById(R.id.title);
        mDescriptionTextView = mMainContentView.findViewById(R.id.description);
        mDescriptionTextView.setMovementMethod(new ScrollingMovementMethod());
        mPostFrequencyTextView = mMainContentView.findViewById(R.id.post_freq);
        mPostsContainer = mMainContentView.findViewById(R.id.posts_container);
        return mMainContentView;
    }

    void setAvatarBitmap(Bitmap avatarBitmap) {
        if (avatarBitmap != null) mAvatarView.setImageBitmap(avatarBitmap);
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
            show();
        } else {
            dismiss();
        }
    }

    void setPosts(ArrayList<ContentPreviewPostData> postsDataList) {
        if (postsDataList == null) return;

        for (ContentPreviewPostData postData : postsDataList) {
            ContentPreviewPostView postView = new ContentPreviewPostView(mContext);
            if (postData.getImageBitmap() != null) {
                postView.setImageBitmap(postData.getImageBitmap());
            }
            postView.setTitle(postData.getTitle());
            // TODO(crbug/1052182): set posted time text according to the mock.
            // TODO(crbug/1052184): add OnClickListener to the post.

            mPostsContainer.addView(postView);
            postView.setVisibility(true);
        }
    }
}

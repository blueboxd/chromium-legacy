// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card.internal;

import android.content.Context;
import android.text.method.ScrollingMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.RectProvider;

/**
 * UI component that handles showing a text bubble with a preceding image.
 */
public class ProfileCardView extends TextBubble {
    private View mMainContentView;
    private TextView mTitleTextView;
    private TextView mDescriptionTextView;
    private ImageButton mFollowBtn;
    private LinearLayout mNetworkContainer;

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
                anchorRectProvider);
        setColor(context.getResources().getColor(org.chromium.chrome.R.color.material_grey_100));
        setCardDismissListener();
    }

    @Override
    protected View createContentView() {
        mMainContentView =
                LayoutInflater.from(mContext).inflate(R.layout.profile_card, /*root=*/null);
        mTitleTextView = mMainContentView.findViewById(R.id.title);
        mDescriptionTextView = mMainContentView.findViewById(R.id.description);
        mDescriptionTextView.setMovementMethod(new ScrollingMovementMethod());
        return mMainContentView;
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

    void setVisibility(boolean visible) {
        if (visible) {
            setOutsideTouchable(true);
            show();
        } else {
            dismiss();
        }
    }

    void setCardDismissListener() {
        addOnDismissListener(new OnDismissListener() {
            @Override
            public void onDismiss() {
                setVisibility(false);
            }
        });
    }
}

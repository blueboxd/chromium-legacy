// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.content.res.Configuration;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.start_surface.R;

/**
 * A {@link LinearLayout} that shows loading placeholder for feed.
 */
public class FeedLoadingLayout extends LinearLayout {
    private Context mContext;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;

    public FeedLoadingLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // TODO (crbug.com/1079443): Inflate article suggestions section header here.
        setImageForConfiguration();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        setImageForConfiguration();
    }

    /** @return The {@link PersonalizedSigninPromoView} for this class. */
    PersonalizedSigninPromoView getSigninPromoView() {
        if (mSigninPromoView == null) {
            mSigninPromoView = (PersonalizedSigninPromoView) LayoutInflater.from(mContext).inflate(
                    R.layout.personalized_signin_promo_view_modern_content_suggestions, null,
                    false);
            LinearLayout signView = findViewById(R.id.sign_in_box);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            signView.setLayoutParams(lp);
            lp.setMargins(0, 0, 0, dpToPx(12));
            signView.addView(mSigninPromoView);
        }
        return mSigninPromoView;
    }

    private void setImageForConfiguration() {
        int currentOrientation = getResources().getConfiguration().orientation;
        LinearLayout imageParentView = findViewById(R.id.images_layout);
        if (currentOrientation == Configuration.ORIENTATION_LANDSCAPE) {
            setPaddingRelative(dpToPx(48), 0, dpToPx(48), 0);
            setImages(imageParentView, 2, R.drawable.feed_loading_landscape_drawable);
        } else {
            setPaddingRelative(dpToPx(12), 0, dpToPx(12), 0);
            setImages(imageParentView, 4, R.drawable.feed_loading_drawable);
        }
    }

    private void setImages(LinearLayout imageParentView, int imageCount, int drawableID) {
        imageParentView.removeAllViews();
        for (int i = 0; i < imageCount; i++) {
            ImageView child = new ImageView(mContext);
            child.setImageResource(drawableID);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            child.setLayoutParams(lp);
            lp.setMargins(0, 0, 0, dpToPx(12));
            child.setScaleType(ImageView.ScaleType.FIT_XY);
            child.setAdjustViewBounds(true);
            child.setBackgroundResource(R.drawable.hairline_border_card_background);
            imageParentView.addView(child);
        }
    }

    private int dpToPx(int dp) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
    }
}

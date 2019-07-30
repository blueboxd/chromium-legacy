// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.os.Build;
import android.support.annotation.DrawableRes;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation.AnimationListener;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.RippleBackgroundHelper;

/**
 * View class for a bubble used in gesture navigation UI that consists of an icon
 * and an optional text.
 */
public class NavigationBubble extends LinearLayout {
    private final RippleBackgroundHelper mRippleBackgroundHelper;

    private ChromeImageView mIcon;
    private TextView mText;
    private AnimationListener mListener;

    /**
     * Constructor for inflating from XML.
     */
    public NavigationBubble(Context context) {
        this(context, null);
    }

    public NavigationBubble(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Workaround to a bug that makes this view sometimes stay invisible after animation.
        // https://stackoverflow.com/questions/24258832
        // https://stackoverflow.com/questions/25099043
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        }

        // Reset icon and background. Height is used as corner radius to ensure we have a circle.
        mRippleBackgroundHelper = new RippleBackgroundHelper(this,
                R.color.navigation_bubble_background_color, R.color.navigation_bubble_ripple_color,
                getResources().getDimensionPixelSize(R.dimen.navigation_bubble_default_height),
                R.color.navigation_bubble_stroke_color, R.dimen.navigation_bubble_border_width,
                getResources().getDimensionPixelSize(R.dimen.navigation_bubble_bg_vertical_inset));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIcon = findViewById(R.id.navigation_bubble_icon);
        mText = findViewById(R.id.navigation_bubble_text);
    }

    /**
     * Sets {@link AnimationListener} used when the widget disappears at the end of
     * user gesture.
     * @param listener Listener object.
     */
    public void setAnimationListener(AnimationListener listener) {
        mListener = listener;
    }

    /**
     * @return {@code true} if the widget is showing the close chrome indicator text.
     */
    public boolean isShowingCaption() {
        return getTextView().getVisibility() == View.VISIBLE;
    }

    /**
     * Shows or hides the close chrome indicator.
     * @param on {@code true} if the indicator should appear.
     */
    public void showCaption(boolean on) {
        if (on && !isShowingCaption()) {
            getTextView().setVisibility(View.VISIBLE);
            // Measure the width again after the indicator text becomes visible.
            measure(0, 0);
        } else if (!on && isShowingCaption()) {
            getTextView().setVisibility(View.GONE);
        }
    }

    @Override
    public void onAnimationStart() {
        super.onAnimationStart();
        if (mListener != null) {
            mListener.onAnimationStart(getAnimation());
        }
    }

    @Override
    public void onAnimationEnd() {
        super.onAnimationEnd();
        if (mListener != null) {
            mListener.onAnimationEnd(getAnimation());
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (mRippleBackgroundHelper != null) {
            mRippleBackgroundHelper.onDrawableStateChanged();
        }
    }

    /**
     * Sets the icon at the start of the icon view.
     * @param icon The resource id pointing to the icon.
     */
    public void setIcon(@DrawableRes int icon) {
        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageResource(icon);

        // Sets the correct tinting on the icon.
        ApiCompatibilityUtils.setImageTintList(mIcon, mText.getTextColors());
    }

    /**
     * Returns the {@link TextView} that contains the label of the widget.
     * @return A {@link TextView}.
     */
    public TextView getTextView() {
        return mText;
    }
}

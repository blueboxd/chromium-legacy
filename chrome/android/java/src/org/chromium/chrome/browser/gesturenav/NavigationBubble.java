// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PorterDuff.Mode;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation.AnimationListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.ColorUtils;

/**
 * View class for a bubble used in gesture navigation UI that consists of an icon
 * and an optional text.
 */
public class NavigationBubble extends LinearLayout {
    private static final int COLOR_TRANSITION_DURATION_MS = 250;

    private static final float FADE_ALPHA = 0.5f;

    private static final int FADE_DURATION_MS = 400;

    private final ValueAnimator mColorAnimator;
    private final int mBlue;
    private final int mBlack;

    private class ColorUpdateListener implements ValueAnimator.AnimatorUpdateListener {
        private int mStart;
        private int mEnd;

        private void setTransitionColors(int start, int end) {
            mStart = start;
            mEnd = end;
        }

        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            float fraction = (float) animation.getAnimatedValue();
            ApiCompatibilityUtils.setImageTintList(mIcon,
                    ColorStateList.valueOf(ColorUtils.getColorWithOverlay(mStart, mEnd, fraction)));
        }
    }

    private final ColorUpdateListener mColorUpdateListener;

    private TextView mText;
    private ImageView mIcon;
    private AnimationListener mListener;

    // True if arrow bubble is faded out.
    private boolean mArrowFaded;

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
        mBlack = ApiCompatibilityUtils.getColor(getResources(), R.color.navigation_bubble_arrow);
        mBlue = ApiCompatibilityUtils.getColor(getResources(), R.color.default_icon_color_blue);

        mColorUpdateListener = new ColorUpdateListener();
        mColorAnimator = ValueAnimator.ofFloat(0, 1).setDuration(COLOR_TRANSITION_DURATION_MS);
        mColorAnimator.addUpdateListener(mColorUpdateListener);
        getBackground().setColorFilter(ApiCompatibilityUtils.getColor(getResources(),
                                               R.color.navigation_bubble_background_color),
                Mode.MULTIPLY);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIcon = findViewById(R.id.navigation_bubble_arrow);
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

    /**
     * Sets the icon at the start of the icon view.
     * @param icon The resource id pointing to the icon.
     */
    public void setIcon(@DrawableRes int icon) {
        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageResource(icon);
        setImageTint(false);
    }

    /**
     * Sets the correct tinting on the arrow icon.
     */
    public void setImageTint(boolean navigate) {
        assert mIcon != null;
        mColorUpdateListener.setTransitionColors(
                navigate ? mBlack : mBlue, navigate ? mBlue : mBlack);
        mColorAnimator.start();
    }

    /**
     * Returns the {@link TextView} that contains the label of the widget.
     * @return A {@link TextView}.
     */
    public TextView getTextView() {
        return mText;
    }

    /**
     * Fade out the arrow bubble.
     * @param faded {@code true} if the bubble should be faded.
     * @param animate {@code true} if animation is needed.
     */
    public void setFaded(boolean faded, boolean animate) {
        if (faded == mArrowFaded) return;
        assert mIcon != null;
        animate().alpha(faded ? FADE_ALPHA : 1.f).setDuration(animate ? FADE_DURATION_MS : 0);
        mArrowFaded = faded;
    }
}

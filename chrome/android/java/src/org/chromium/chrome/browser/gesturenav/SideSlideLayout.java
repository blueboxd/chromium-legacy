// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.support.annotation.IntDef;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.AnimationSet;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.LinearInterpolator;
import android.view.animation.ScaleAnimation;
import android.view.animation.Transformation;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The SideSlideLayout can be used whenever the user navigates the contents
 * of a view using horizontal gesture. Shows an arrow widget moving horizontally
 * in reaction to the gesture which, if goes over a threshold, triggers navigation.
 * The caller that instantiates this view should add an {@link #OnNavigateListener}
 * to be notified whenever the gesture is completed.
 * Based on {@link org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout}
 * and modified accordingly to support horizontal gesture.
 */
public class SideSlideLayout extends ViewGroup {
    private static final String NAVIGATION_SHEET_ENABLED_KEY =
            "overscroll_history_navigation_bottom_sheet";
    // Used to record the UMA histogram Overscroll.* This definition should be
    // in sync with that in content/browser/web_contents/aura/types.h
    // TODO(jinsukkim): Generate java enum from the native header.
    @IntDef({UmaNavigationType.NAVIGATION_TYPE_NONE, UmaNavigationType.FORWARD_TOUCHPAD,
            UmaNavigationType.BACK_TOUCHPAD, UmaNavigationType.FORWARD_TOUCHSCREEN,
            UmaNavigationType.BACK_TOUCHSCREEN, UmaNavigationType.RELOAD_TOUCHPAD,
            UmaNavigationType.RELOAD_TOUCHSCREEN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface UmaNavigationType {
        int NAVIGATION_TYPE_NONE = 0;
        int FORWARD_TOUCHPAD = 1;
        int BACK_TOUCHPAD = 2;
        int FORWARD_TOUCHSCREEN = 3;
        int BACK_TOUCHSCREEN = 4;
        int RELOAD_TOUCHPAD = 5;
        int RELOAD_TOUCHSCREEN = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Classes that wish to be notified when the swipe gesture correctly
     * triggers navigation should implement this interface.
     */
    public interface OnNavigateListener { void onNavigate(boolean isForward); }

    /**
     * Classes that wish to be notified when a reset is triggered should
     * implement this interface.
     */
    public interface OnResetListener { void onReset(); }

    private static final int CIRCLE_DIAMETER_DP = 40;

    // Offset in dips from the border of the view. Gesture triggers the navigation
    // if slid by this amount or more.
    private static final int TARGET_THRESHOLD_DP = 64;

    private static final float DECELERATE_INTERPOLATION_FACTOR = 2f;

    private static final int SCALE_DOWN_DURATION_MS = 400;
    private static final int ANIMATE_TO_START_DURATION_MS = 500;

    // Minimum number of pull updates necessary to trigger a side nav.
    private static final int MIN_PULLS_TO_ACTIVATE = 3;

    private final DecelerateInterpolator mDecelerateInterpolator;
    private final LinearInterpolator mLinearInterpolator;
    private final float mTotalDragDistance;
    private final int mMediumAnimationDuration;
    private final int mCircleWidth;

    private OnNavigateListener mListener;
    private OnResetListener mResetListener;

    // Flag indicating that the navigation will be activated.
    private boolean mNavigating;

    private int mCurrentTargetOffset;
    private float mTotalMotion;

    // Whether or not the starting offset has been determined.
    private boolean mOriginalOffsetCalculated;

    // True while side gesture is in progress.
    private boolean mIsBeingDragged;

    private NavigationBubble mArrowView;
    private int mArrowViewWidth;

    // Start position for animation moving the UI back to original offset.
    private int mFrom;
    private int mOriginalOffset;

    private AnimationSet mHidingAnimation;
    private int mAnimationViewWidth;
    private AnimationListener mCancelAnimationListener;

    private boolean mIsForward;
    private boolean mCloseIndicatorEnabled;

    private final AnimationListener mNavigateListener = new AnimationListener() {
        @Override
        public void onAnimationStart(Animation animation) {}

        @Override
        public void onAnimationRepeat(Animation animation) {}

        @Override
        public void onAnimationEnd(Animation animation) {
            mArrowView.setVisibility(View.INVISIBLE);
            if (mNavigating) {
                if (mListener != null) mListener.onNavigate(mIsForward);
                recordHistogram("Overscroll.Navigated3", mIsForward);
            } else {
                reset();
            }
            hideCloseIndicator();
        }
    };

    private final Animation mAnimateToStartPosition = new Animation() {
        @Override
        public void applyTransformation(float interpolatedTime, Transformation t) {
            int targetTop = mFrom + (int) ((mOriginalOffset - mFrom) * interpolatedTime);
            int offset = targetTop - mArrowView.getLeft();
            mTotalMotion += offset;

            float progress = Math.min(1.f, getOverscroll() / mTotalDragDistance);
            setTargetOffsetLeftAndRight(offset);
        }
    };

    public SideSlideLayout(Context context) {
        super(context);

        mMediumAnimationDuration =
                getResources().getInteger(android.R.integer.config_mediumAnimTime);

        setWillNotDraw(false);
        mDecelerateInterpolator = new DecelerateInterpolator(DECELERATE_INTERPOLATION_FACTOR);
        mLinearInterpolator = new LinearInterpolator();

        final float density = getResources().getDisplayMetrics().density;
        mCircleWidth = (int) (CIRCLE_DIAMETER_DP * density);

        LayoutInflater layoutInflater = LayoutInflater.from(getContext());
        mArrowView = (NavigationBubble) layoutInflater.inflate(R.layout.navigation_bubble, null);
        mArrowView.getTextView().setText(
                getResources().getString(R.string.overscroll_navigation_close_chrome,
                        getContext().getString(R.string.app_name)));
        mArrowViewWidth = mCircleWidth;
        addView(mArrowView);

        // The absolute offset has to take into account that the circle starts at an offset
        mTotalDragDistance = TARGET_THRESHOLD_DP * density;
    }

    private static boolean isBottomSheetEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION, NAVIGATION_SHEET_ENABLED_KEY,
                false);
    }

    /**
     * Set the listener to be notified when the navigation is triggered.
     */
    public void setOnNavigationListener(OnNavigateListener listener) {
        mListener = listener;
    }

    /**
     * Set the reset listener to be notified when a reset is triggered.
     */
    public void setOnResetListener(OnResetListener listener) {
        mResetListener = listener;
    }

    /**
     * Stop navigation.
     */
    public void stopNavigating() {
        setNavigating(false);
    }

    private void setNavigating(boolean navigating) {
        if (mNavigating != navigating) {
            mNavigating = navigating;
            if (mNavigating) startHidingAnimation(mNavigateListener);
        }
    }

    private float getOverscroll() {
        return mIsForward ? -Math.min(0, mTotalMotion) : Math.max(0, mTotalMotion);
    }

    private void startHidingAnimation(AnimationListener listener) {
        // ScaleAnimation needs to be created again if the arrow widget width changes over time
        // (due to turning on/off close indicator) to set the right x pivot point.
        if (mHidingAnimation == null || mAnimationViewWidth != mArrowViewWidth) {
            mAnimationViewWidth = mArrowViewWidth;
            ScaleAnimation scalingDown =
                    new ScaleAnimation(1, 0, 1, 0, mArrowViewWidth / 2, mArrowView.getHeight() / 2);
            scalingDown.setInterpolator(mLinearInterpolator);
            scalingDown.setDuration(SCALE_DOWN_DURATION_MS);
            Animation fadingOut = new AlphaAnimation(1, 0);
            fadingOut.setInterpolator(mDecelerateInterpolator);
            fadingOut.setDuration(SCALE_DOWN_DURATION_MS);
            mHidingAnimation = new AnimationSet(false);
            mHidingAnimation.addAnimation(fadingOut);
            mHidingAnimation.addAnimation(scalingDown);
        }
        mArrowView.setAnimationListener(listener);
        mArrowView.clearAnimation();
        mArrowView.startAnimation(mHidingAnimation);
    }

    /**
     * Set the direction used for sliding gesture.
     * @param forward {@code true} if direction is forward.
     */
    public void setDirection(boolean forward) {
        mIsForward = forward;
        mArrowView.setIcon(
                forward ? R.drawable.ic_arrow_forward_blue_24dp : R.drawable.ic_arrow_back_24dp);
    }

    public void setEnableCloseIndicator(boolean enable) {
        mCloseIndicatorEnabled = enable;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        if (getChildCount() == 0) return;

        final int height = getMeasuredHeight();
        final int arrowWidth = mArrowView.getMeasuredWidth();
        final int arrowHeight = mArrowView.getMeasuredHeight();
        mArrowView.layout(mCurrentTargetOffset, height / 2 - arrowHeight / 2,
                mCurrentTargetOffset + arrowWidth, height / 2 + arrowHeight / 2);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mArrowView.measure(MeasureSpec.makeMeasureSpec(mArrowViewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mCircleWidth, MeasureSpec.EXACTLY));
        if (!mOriginalOffsetCalculated) {
            initializeOffset();
            mOriginalOffsetCalculated = true;
        }
    }

    private void initializeOffset() {
        mCurrentTargetOffset = mOriginalOffset = mIsForward ? getMeasuredWidth() : -mArrowViewWidth;
    }

    /**
     * Start the pull effect. If the effect is disabled or a navigation animation
     * is currently active, the request will be ignored.
     * @return whether a new pull sequence has started.
     */
    public boolean start() {
        if (!isEnabled() || mNavigating || mListener == null) return false;
        mArrowView.clearAnimation();
        mTotalMotion = 0;
        mIsBeingDragged = true;
        initializeOffset();
        return true;
    }

    /**
     * Apply a pull impulse to the effect. If the effect is disabled or has yet
     * to start, the pull will be ignored.
     * @param delta the magnitude of the pull.
     */
    public void pull(float delta) {
        if (!isEnabled() || !mIsBeingDragged) return;

        float maxDelta = mTotalDragDistance / MIN_PULLS_TO_ACTIVATE;
        delta = Math.max(-maxDelta, Math.min(maxDelta, delta));
        mTotalMotion += delta;

        float overscroll = getOverscroll();
        float extraOs = overscroll - mTotalDragDistance;
        float slingshotDist = mTotalDragDistance;
        float tensionSlingshotPercent =
                Math.max(0, Math.min(extraOs, slingshotDist * 2) / slingshotDist);
        float tensionPercent =
                (float) ((tensionSlingshotPercent / 4) - Math.pow((tensionSlingshotPercent / 4), 2))
                * 2f;

        if (mArrowView.getVisibility() != View.VISIBLE) mArrowView.setVisibility(View.VISIBLE);

        float originalDragPercent = overscroll / mTotalDragDistance;
        float dragPercent = Math.min(1f, Math.abs(originalDragPercent));

        if (mCloseIndicatorEnabled) {
            if (getOverscroll() > mTotalDragDistance) {
                mArrowView.showCaption(true);
                mArrowViewWidth = mArrowView.getMeasuredWidth();
            } else {
                hideCloseIndicator();
            }
        }

        float extraMove = slingshotDist * tensionPercent * 2;
        int targetDiff = (int) (slingshotDist * dragPercent + extraMove);
        int targetX = mOriginalOffset + (mIsForward ? -targetDiff : targetDiff);
        setTargetOffsetLeftAndRight(targetX - mCurrentTargetOffset);
    }

    private void hideCloseIndicator() {
        mArrowView.showCaption(false);
        // The width when indicator text view is hidden is slightly bigger than the height.
        // Set the width to circle's diameter for the widget to be of completely round shape.
        mArrowViewWidth = mCircleWidth;
    }

    private void setTargetOffsetLeftAndRight(int offset) {
        mArrowView.offsetLeftAndRight(offset);
        mCurrentTargetOffset = mArrowView.getLeft();
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav whether to allow a sufficiently large pull to trigger
     *                     the navigation action and animation sequence.
     */
    public void release(boolean allowNav) {
        if (!mIsBeingDragged) return;

        // See ACTION_UP handling in {@link #onTouchEvent(...)}.
        mIsBeingDragged = false;
        if (isEnabled() && allowNav && getOverscroll() > mTotalDragDistance) {
            setNavigating(true);
            return;
        }
        // Cancel navigation
        mNavigating = false;
        if (mCancelAnimationListener == null) {
            mCancelAnimationListener = new AnimationListener() {
                @Override
                public void onAnimationStart(Animation animation) {}

                @Override
                public void onAnimationEnd(Animation animation) {
                    startHidingAnimation(mNavigateListener);
                }

                @Override
                public void onAnimationRepeat(Animation animation) {}
            };
        }
        mFrom = mCurrentTargetOffset;
        mAnimateToStartPosition.reset();
        mAnimateToStartPosition.setDuration(ANIMATE_TO_START_DURATION_MS);
        mAnimateToStartPosition.setInterpolator(mDecelerateInterpolator);
        mArrowView.setAnimationListener(mCancelAnimationListener);
        mArrowView.clearAnimation();
        mArrowView.startAnimation(mAnimateToStartPosition);

        recordHistogram("Overscroll.Cancelled3", mIsForward);
    }

    /**
     * Reset the effect, clearing any active animations.
     */
    public void reset() {
        mIsBeingDragged = false;
        setNavigating(false);
        hideCloseIndicator();

        // Return the circle to its start position
        setTargetOffsetLeftAndRight(mOriginalOffset - mCurrentTargetOffset);
        mCurrentTargetOffset = mArrowView.getLeft();
        if (mResetListener != null) mResetListener.onReset();
    }

    private static void recordHistogram(String name, boolean forward) {
        RecordHistogram.recordEnumeratedHistogram(name,
                forward ? UmaNavigationType.FORWARD_TOUCHSCREEN
                        : UmaNavigationType.BACK_TOUCHSCREEN,
                UmaNavigationType.NUM_ENTRIES);
    }
}

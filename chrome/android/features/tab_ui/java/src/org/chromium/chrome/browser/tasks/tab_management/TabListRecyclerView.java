// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class.
 */
class TabListRecyclerView extends RecyclerView {
    public static final long BASE_ANIMATION_DURATION_MS = 218;
    public static final long FINAL_FADE_IN_DURATION_MS = 50;
    public static final long RESTORE_ANIMATION_DURATION_MS = 10;
    public static final int ANIMATION_STATUS_RESTORE = 0;
    public static final int ANIMATION_STATUS_ZOOM_OUT = 1;
    public static final int ANIMATION_STATUS_ZOOM_IN = 2;

    /**
     * Field trial parameter for downsampling scaling factor.
     */
    private static final String DOWNSAMPLING_SCALE_PARAM = "downsampling-scale";

    private static final float DEFAULT_DOWNSAMPLING_SCALE = 0.5f;

    /**
     * An interface to listen to visibility related changes on this {@link RecyclerView}.
     */
    interface VisibilityListener {
        /**
         * Called before the animation to show the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedShowing(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedShowing();

        /**
         * Called before the animation to hide the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedHiding(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedHiding();
    }

    private class TabListOnScrollListener extends RecyclerView.OnScrollListener {
        // TODO(mattsimmons): Remove state from this class. This is here to prevent scroll signals
        //  from showing the toolbar shadow after the show/hide of the GTS has already triggered the
        //  shadow being hidden. Due to this view's visibility being updated asynchronously after
        //  animating off-screen, checking View.getVisibility() can't be used as the guard condition
        //  here. Removing/Adding the listener from the view also happens asynchronously and has
        //  similar undesirable effects.
        private boolean mIsActive;

        public void pause() {
            mIsActive = false;
        }

        public void resume() {
            mIsActive = true;
        }

        @Override
        public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
            if (!mIsActive) return;

            final int yOffset = recyclerView.computeVerticalScrollOffset();
            setShadowVisibility(yOffset > 0);
        }
    }

    private ValueAnimator mFadeInAnimator;
    private ValueAnimator mFadeOutAnimator;
    private VisibilityListener mListener;
    private DynamicResourceLoader mLoader;
    private ViewResourceAdapter mDynamicView;
    private boolean mIsDynamicViewRegistered;
    private long mLastDirtyTime;
    private RecyclerView.ItemAnimator mOriginalAnimator;
    private ImageView mShadowImageView;
    private int mShadowTopMargin;
    private TabListOnScrollListener mScrollListener;

    /**
     * Basic constructor to use during inflation from xml.
     */
    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    /**
     * Set the {@link VisibilityListener} that will listen on granular visibility events.
     * @param listener The {@link VisibilityListener} to use.
     */
    void setVisibilityListener(VisibilityListener listener) {
        mListener = listener;
    }

    void prepareOverview() {
        endAllAnimations();

        registerDynamicView();

        // Stop all the animations to make all the items show up and scroll to position immediately.
        mOriginalAnimator = getItemAnimator();
        setItemAnimator(null);
    }

    /**
     * Start showing the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startShowing(boolean animate) {
        assert mFadeOutAnimator == null;
        mListener.startedShowing(animate);

        long duration = ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                ? FINAL_FADE_IN_DURATION_MS
                : BASE_ANIMATION_DURATION_MS;

        setAlpha(0);
        setVisibility(View.VISIBLE);
        mFadeInAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 1);
        mFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mFadeInAnimator.setDuration(duration);
        mFadeInAnimator.start();
        mFadeInAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeInAnimator = null;
                mListener.finishedShowing();
                // Restore the original value.
                setItemAnimator(mOriginalAnimator);
                mScrollListener.resume();
                setShadowVisibility(computeVerticalScrollOffset() > 0);
                if (mDynamicView != null) {
                    mDynamicView.dropCachedBitmap();
                    unregisterDynamicView();
                }
            }
        });
        if (!animate) mFadeInAnimator.end();
    }

    void setShadowVisibility(boolean shouldShowShadow) {
        if (!(getParent() instanceof FrameLayout)) return;

        if (mShadowImageView == null) {
            Context context = getContext();
            mShadowImageView = new ImageView(context);
            mShadowImageView.setImageDrawable(
                    AppCompatResources.getDrawable(context, R.drawable.modern_toolbar_shadow));
            Resources res = context.getResources();
            FrameLayout.LayoutParams params =
                    new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                            res.getDimensionPixelSize(R.dimen.toolbar_shadow_height), Gravity.TOP);
            params.topMargin = mShadowTopMargin;
            mShadowImageView.setScaleType(ImageView.ScaleType.FIT_XY);
            mShadowImageView.setLayoutParams(params);
            FrameLayout parent = (FrameLayout) getParent();
            parent.addView(mShadowImageView);
        }

        if (shouldShowShadow && mShadowImageView.getVisibility() != VISIBLE) {
            mShadowImageView.setVisibility(VISIBLE);
        } else if (!shouldShowShadow && mShadowImageView.getVisibility() != GONE) {
            mShadowImageView.setVisibility(GONE);
        }
    }

    void setShadowTopMargin(int shadowTopMargin) {
        mShadowTopMargin = shadowTopMargin;
    }

    /**
     * @return The ID for registering and using the dynamic resource in compositor.
     */
    int getResourceId() {
        return getId();
    }

    long getLastDirtyTimeForTesting() {
        return mLastDirtyTime;
    }

    private float getDownsamplingScale() {
        String scale = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, DOWNSAMPLING_SCALE_PARAM);
        try {
            return Float.valueOf(scale);
        } catch (NumberFormatException e) {
            return DEFAULT_DOWNSAMPLING_SCALE;
        }
    }

    /**
     * Create a DynamicResource for this RecyclerView.
     * The view resource can be obtained by {@link #getResourceId} in compositor layer.
     */
    void createDynamicView(DynamicResourceLoader loader) {
        mDynamicView = new ViewResourceAdapter(this) {
            @Override
            public boolean isDirty() {
                boolean dirty = super.isDirty();
                if (dirty) mLastDirtyTime = SystemClock.elapsedRealtime();
                return dirty;
            }
        };
        mDynamicView.setDownsamplingScale(getDownsamplingScale());
        assert mLoader == null : "createDynamicView should only be called once";
        mLoader = loader;
    }

    private void registerDynamicView() {
        if (mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.registerResource(getResourceId(), mDynamicView);
        mIsDynamicViewRegistered = true;
    }

    private void unregisterDynamicView() {
        if (!mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.unregisterResource(getResourceId());
        mIsDynamicViewRegistered = false;
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        if (mDynamicView != null) {
            mDynamicView.invalidate(null);
        }
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        ViewParent retVal = super.invalidateChildInParent(location, dirty);
        if (mDynamicView != null) {
            mDynamicView.invalidate(dirty);
        }
        return retVal;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mScrollListener = new TabListOnScrollListener();
        addOnScrollListener(mScrollListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mShadowImageView != null) {
            removeViewInLayout(mShadowImageView);
            mShadowImageView = null;
        }

        if (mScrollListener != null) {
            removeOnScrollListener(mScrollListener);
            mScrollListener = null;
        }
    }

    /**
     * Start hiding the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startHiding(boolean animate) {
        endAllAnimations();

        registerDynamicView();

        mListener.startedHiding(animate);
        mFadeOutAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 0);
        mFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mFadeOutAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeOutAnimator = null;
                setVisibility(View.INVISIBLE);
                mListener.finishedHiding();
            }
        });
        mScrollListener.pause();
        setShadowVisibility(false);
        mFadeOutAnimator.start();
        if (!animate) mFadeOutAnimator.end();
    }

    void postHiding() {
        if (mDynamicView != null) {
            mDynamicView.dropCachedBitmap();
            unregisterDynamicView();
        }
    }

    private void endAllAnimations() {
        if (mFadeInAnimator != null) {
            mFadeInAnimator.end();
        }
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.end();
        }
    }

    /**
     * @param currentTabIndex The the current tab's index in the model.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         {@link TabListRecyclerView} coordinates.
     */
    @Nullable
    Rect getRectOfCurrentThumbnail(int currentTabIndex) {
        TabGridViewHolder holder =
                (TabGridViewHolder) findViewHolderForAdapterPosition(currentTabIndex);
        if (holder == null) return null;

        int[] loc = new int[2];
        holder.thumbnail.getLocationInWindow(loc);
        Rect rect = new Rect(loc[0], loc[1], loc[0] + holder.thumbnail.getWidth(),
                loc[1] + holder.thumbnail.getHeight());
        getLocationInWindow(loc);
        rect.top -= loc[1];
        rect.bottom -= loc[1];
        return rect;
    }

    static void scaleTabGridCardView(View view, int status) {
        AnimatorSet scaleAnimator = new AnimatorSet();
        float scale = status == ANIMATION_STATUS_ZOOM_IN ? 0.8f : 1f;
        ObjectAnimator scaleX = ObjectAnimator.ofFloat(view, "scaleX", scale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(view, "scaleY", scale);
        scaleX.setDuration(status == ANIMATION_STATUS_RESTORE ? RESTORE_ANIMATION_DURATION_MS
                                                              : BASE_ANIMATION_DURATION_MS);
        scaleY.setDuration(status == ANIMATION_STATUS_RESTORE ? RESTORE_ANIMATION_DURATION_MS
                                                              : BASE_ANIMATION_DURATION_MS);
        scaleAnimator.play(scaleX).with(scaleY);
        scaleAnimator.start();
    }

    /**
     * This method finds out the index of the hovered tab's viewHolder in {@code recyclerView}.
     * @param recyclerView   The recyclerview that owns the tabs' viewHolders.
     * @param view           The view of the selected tab.
     * @param dX             The X offset of the selected tab.
     * @param dY             The Y offset of the selected tab.
     * @param threshold      The threshold to judge whether two tabs are overlapped.
     * @return The index of the hovered tab.
     */
    static int getHoveredTabIndex(
            RecyclerView recyclerView, View view, float dX, float dY, float threshold) {
        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View child = recyclerView.getChildAt(i);
            if (child.getLeft() == view.getLeft() && child.getTop() == view.getTop()) {
                continue;
            }
            if (isOverlap(child.getLeft(), child.getTop(), view.getLeft() + dX, view.getTop() + dY,
                        threshold)) {
                return i;
            }
        }
        return -1;
    }

    private static boolean isOverlap(
            float left1, float top1, float left2, float top2, float threshold) {
        return Math.abs(left1 - left2) < threshold && Math.abs(top1 - top2) < threshold;
    }

    /**
     * This method gets the position of certain item in the {@link TabListRecyclerView}.
     *
     * @param index  The index of the item whose position is requested.
     * @return The {@link Rect} that contains the position information.
     */
    Rect getTabPosition(int index) {
        Rect rect = new Rect();
        View holder = findViewHolderForAdapterPosition(index).itemView;
        holder.getGlobalVisibleRect(rect);
        return rect;
    }
}

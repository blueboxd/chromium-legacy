// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.support.annotation.IntDef;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Parent for TabGridDialog component.
 * TODO(yuezhanggg): Add animations of card scales up to dialog and dialog scales down to card when
 * show/hide dialog.
 */
public class TabGridDialogParent {
    private static final int DIALOG_ANIMATION_DURATION = 300;
    private static final int DIALOG_QUICK_FADEOUT_DURATION = 50;
    @IntDef({UngroupBarStatus.SHOW, UngroupBarStatus.HIDE, UngroupBarStatus.HOVERED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UngroupBarStatus {
        int SHOW = 0;
        int HIDE = 1;
        int HOVERED = 2;
        int NUM_ENTRIES = 3;
    }

    private final ComponentCallbacks mComponentCallbacks;
    private final FrameLayout.LayoutParams mContainerParams;
    private final ViewGroup mParent;
    private final int mToolbarHeight;
    private final int mScreenHeight;
    private final int mScreenWidth;
    private final float mTabGridCardPadding;
    private PopupWindow mPopupWindow;
    private FrameLayout mTabGridDialogParentView;
    private RelativeLayout mDialogContainerView;
    private ScrimView mScrimView;
    private ScrimView.ScrimParams mScrimParams;
    private View mBlockView;
    private View mContentView;
    private TextView mUngroupBar;
    private Animator mCurrentAnimator;
    private ObjectAnimator mBasicFadeIn;
    private ObjectAnimator mBasicFadeOut;
    private AnimatorSet mShowDialogAnimation;
    private AnimatorSet mHideDialogAnimation;
    private AnimatorListenerAdapter mShowDialogAnimationListener;
    private AnimatorListenerAdapter mHideDialogAnimationListener;
    private int mSideMargin;
    private int mTopMargin;
    private int mCurrentScreenHeight;
    private int mCurrentScreenWidth;

    TabGridDialogParent(Context context, ViewGroup parent) {
        mParent = parent;
        mShowDialogAnimation = new AnimatorSet();
        mHideDialogAnimation = new AnimatorSet();
        mTabGridCardPadding = context.getResources().getDimension(R.dimen.tab_list_card_padding);
        mToolbarHeight =
                (int) context.getResources().getDimension(R.dimen.tab_group_toolbar_height);
        mContainerParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(displayMetrics);
        // Screen height and width when in portrait mode.
        mScreenHeight = Math.max(displayMetrics.heightPixels, displayMetrics.widthPixels);
        mScreenWidth = Math.min(displayMetrics.heightPixels, displayMetrics.widthPixels);

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                updateDialogWithOrientation(context, newConfig.orientation);
            }

            @Override
            public void onLowMemory() {}
        };
        ContextUtils.getApplicationContext().registerComponentCallbacks(mComponentCallbacks);
        setupDialogContent(context);
        prepareAnimation();
    }

    private void setupDialogContent(Context context) {
        mTabGridDialogParentView = (FrameLayout) LayoutInflater.from(context).inflate(
                R.layout.tab_grid_dialog_layout, mParent, false);
        mDialogContainerView = mTabGridDialogParentView.findViewById(R.id.dialog_container_view);
        mDialogContainerView.setLayoutParams(mContainerParams);
        mUngroupBar = mTabGridDialogParentView.findViewById(R.id.dialog_ungroup_bar);
        mBlockView = mTabGridDialogParentView.findViewById(R.id.dialog_block_view);

        mScrimView = new ScrimView(context, null, mTabGridDialogParentView);
        mPopupWindow = new PopupWindow(mTabGridDialogParentView,
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        updateDialogWithOrientation(context, context.getResources().getConfiguration().orientation);
    }

    private void prepareAnimation() {
        mBasicFadeIn = ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        mBasicFadeIn.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mBasicFadeIn.setDuration(DIALOG_ANIMATION_DURATION);

        mBasicFadeOut = ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        mBasicFadeOut.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mBasicFadeOut.setDuration(FeatureUtilities.isTabToGtsAnimationEnabled()
                        ? DIALOG_QUICK_FADEOUT_DURATION
                        : DIALOG_ANIMATION_DURATION);

        mShowDialogAnimationListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mBlockView.setClickable(false);
                mCurrentAnimator = null;
            }
        };
        mHideDialogAnimationListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mBlockView.setClickable(false);
                mPopupWindow.dismiss();
                mCurrentAnimator = null;
                mDialogContainerView.setAlpha(1f);
            }
        };
    }

    void setupDialogAnimation(Rect rect) {
        // In case where user jumps to a new page from dialog, clean existing animations in
        // mHideDialogAnimation and play basic fade out instead of zooming back to corresponding tab
        // grid card.
        if (rect == null) {
            mShowDialogAnimation = new AnimatorSet();
            mShowDialogAnimation.play(mBasicFadeIn);
            mShowDialogAnimation.removeAllListeners();
            mShowDialogAnimation.addListener(mShowDialogAnimationListener);

            mHideDialogAnimation = new AnimatorSet();
            mHideDialogAnimation.play(mBasicFadeOut);
            mHideDialogAnimation.removeAllListeners();
            mHideDialogAnimation.addListener(mHideDialogAnimationListener);
            return;
        }

        int statusBarHeight = mCurrentScreenHeight - mParent.getHeight();
        int dialogHeight = mCurrentScreenHeight - 2 * mTopMargin - statusBarHeight;
        int dialogWidth = mCurrentScreenWidth - 2 * mSideMargin;

        float sourceLeft = rect.left + mTabGridCardPadding;
        float sourceTop = rect.top + mToolbarHeight + mTabGridCardPadding;
        float sourceHeight = rect.height() - 2 * mTabGridCardPadding;
        float sourceWidth = rect.width() - 2 * mTabGridCardPadding;

        float initYPosition = -(dialogHeight / 2 + mTopMargin - sourceHeight / 2 - sourceTop);
        float initXPosition = -(dialogWidth / 2 + mSideMargin - sourceWidth / 2 - sourceLeft);
        float initYScale = sourceHeight / dialogHeight;
        float initXScale = sourceWidth / dialogWidth;
        float scaleOffset = 0.02f;

        final ObjectAnimator showMoveYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, "translationY", initYPosition, 0f);
        showMoveYAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator showMoveXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, "translationX", initXPosition, 0f);
        showMoveXAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator showScaleYAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.SCALE_Y, initYScale - scaleOffset, 1f);
        showScaleYAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator showScaleXAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.SCALE_X, initXScale - scaleOffset, 1f);
        showScaleXAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        mShowDialogAnimation = new AnimatorSet();
        mShowDialogAnimation.play(showMoveXAnimator)
                .with(showMoveYAnimator)
                .with(showScaleXAnimator)
                .with(showScaleYAnimator);
        mShowDialogAnimation.removeAllListeners();
        mShowDialogAnimation.addListener(mShowDialogAnimationListener);

        final ObjectAnimator hideMoveYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, "translationY", 0f, initYPosition);
        hideMoveYAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator hideMoveXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, "translationX", 0f, initXPosition);
        hideMoveXAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator hideScaleYAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.SCALE_Y, 1f, initYScale - scaleOffset);
        hideScaleYAnimator.setDuration(DIALOG_ANIMATION_DURATION);
        final ObjectAnimator hideScaleXAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.SCALE_X, 1f, initXScale - scaleOffset);
        hideScaleXAnimator.setDuration(DIALOG_ANIMATION_DURATION);

        final ObjectAnimator scaleAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        scaleAnimator.setDuration(TabListRecyclerView.BASE_ANIMATION_DURATION_MS);
        scaleAnimator.setStartDelay(DIALOG_ANIMATION_DURATION);
        mHideDialogAnimation = new AnimatorSet();
        mHideDialogAnimation.play(hideMoveXAnimator)
                .with(hideMoveYAnimator)
                .with(hideScaleXAnimator)
                .with(hideScaleYAnimator)
                .with(scaleAnimator);
        mHideDialogAnimation.removeAllListeners();
        mHideDialogAnimation.addListener(mHideDialogAnimationListener);
    }

    @VisibleForTesting
    void updateDialogWithOrientation(Context context, int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mSideMargin =
                    (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
            mTopMargin =
                    (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
            mCurrentScreenHeight = mScreenHeight;
            mCurrentScreenWidth = mScreenWidth;
        } else {
            mSideMargin =
                    (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
            mTopMargin =
                    (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
            mCurrentScreenWidth = mScreenHeight;
            mCurrentScreenHeight = mScreenWidth;
        }
        mContainerParams.setMargins(mSideMargin, mTopMargin, mSideMargin, mTopMargin);
    }

    /**
     * Setup mScrimParams with the {@code scrimViewObserver}.
     *
     * @param scrimViewObserver The ScrimObserver to be used to setup mScrimParams.
     */
    void setScrimViewObserver(ScrimView.ScrimObserver scrimViewObserver) {
        mScrimParams =
                new ScrimView.ScrimParams(mDialogContainerView, false, true, 0, scrimViewObserver);
    }

    /**
     * Reset the dialog content with {@code toolbarView} and {@code recyclerView}.
     *
     * @param toolbarView The toolbarview to be added to dialog.
     * @param recyclerView The recyclerview to be added to dialog.
     */
    void resetDialog(View toolbarView, View recyclerView) {
        mContentView = recyclerView;
        mDialogContainerView.removeAllViews();
        mDialogContainerView.addView(toolbarView);
        mDialogContainerView.addView(mContentView);
        mDialogContainerView.addView(mUngroupBar);
        RelativeLayout.LayoutParams params =
                (RelativeLayout.LayoutParams) recyclerView.getLayoutParams();
        params.setMargins(0, mToolbarHeight, 0, 0);
        recyclerView.setVisibility(View.VISIBLE);
    }

    /**
     * Show {@link PopupWindow} for dialog with animation.
     */
    void showDialog() {
        if (mCurrentAnimator != null && mCurrentAnimator != mShowDialogAnimation) {
            mCurrentAnimator.end();
        }
        mCurrentAnimator = mShowDialogAnimation;
        if (mScrimParams != null) {
            mScrimView.showScrim(mScrimParams);
        }
        mPopupWindow.showAtLocation(mParent, Gravity.CENTER, 0, 0);
        mBlockView.setClickable(true);
        mShowDialogAnimation.start();
    }

    /**
     * Hide {@link PopupWindow} for dialog with animation.
     */
    void hideDialog() {
        if (mCurrentAnimator != null && mCurrentAnimator != mHideDialogAnimation) {
            mCurrentAnimator.end();
        }
        mCurrentAnimator = mHideDialogAnimation;
        mScrimView.hideScrim(true);
        mBlockView.setClickable(true);
        mHideDialogAnimation.start();
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        ContextUtils.getApplicationContext().unregisterComponentCallbacks(mComponentCallbacks);
    }

    /**
     * Update the ungroup bar based on {@code status}.
     *
     * @param status The status in {@link TabGridDialogParent.UngroupBarStatus} that the ungroup bar
     *         should be updated to.
     */
    public void updateUngroupBar(int status) {
        if (status == UngroupBarStatus.SHOW) {
            mUngroupBar.setVisibility(View.VISIBLE);
            mUngroupBar.setAlpha(1f);
            mUngroupBar.bringToFront();
        } else if (status == UngroupBarStatus.HIDE) {
            mUngroupBar.setVisibility(View.INVISIBLE);
        } else if (status == UngroupBarStatus.HOVERED) {
            mUngroupBar.setAlpha(0.7f);
            mContentView.bringToFront();
        }
    }

    @VisibleForTesting
    PopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }

    @VisibleForTesting
    FrameLayout getTabGridDialogParentViewForTesting() {
        return mTabGridDialogParentView;
    }

    @VisibleForTesting
    Animator getCurrentAnimatorForTesting() {
        return mCurrentAnimator;
    }
}

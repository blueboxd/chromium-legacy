// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;

/**
 * Represents a parent component for TabGridIph related UIs.
 */
public class TabGridIphDialogParent {
    private final int mDialogSideMargin;
    private final int mDialogTextSideMargin;
    private final int mDialogTextTopMarginPortrait;
    private final int mDialogTextTopMarginLandscape;
    private final Context mContext;
    private final View mParentView;
    private View mIphDialogView;
    private TextView mCloseIPHDialogButton;
    private ScrimView mScrimView;
    private ScrimView.ScrimParams mScrimParams;
    private Drawable mIphDrawable;
    private PopupWindow mIphWindow;
    private Animatable mIphAnimation;
    private Animatable2Compat.AnimationCallback mAnimationCallback;
    private ViewGroup.MarginLayoutParams mDialogMarginParams;
    private ViewGroup.MarginLayoutParams mTitleTextMarginParams;
    private ViewGroup.MarginLayoutParams mDescriptionTextMarginParams;
    private ViewGroup.MarginLayoutParams mCloseButtonMarginParams;
    private ComponentCallbacks mComponentCallbacks;

    TabGridIphDialogParent(Context context, View parentView) {
        mContext = context;
        mParentView = parentView;
        mDialogSideMargin =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_side_margin);
        mDialogTextSideMargin = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_side_margin);
        mDialogTextTopMarginPortrait = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_portrait);
        mDialogTextTopMarginLandscape = (int) mContext.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_landscape);

        FrameLayout backgroundView = new FrameLayout(mContext);
        mIphDialogView = LayoutInflater.from(mContext).inflate(
                R.layout.iph_drag_and_drop_dialog_layout, backgroundView, false);
        mCloseIPHDialogButton = mIphDialogView.findViewById(R.id.close_button);
        mIphDrawable =
                ((ImageView) mIphDialogView.findViewById(R.id.animation_drawable)).getDrawable();
        mIphAnimation = (Animatable) mIphDrawable;
        TextView iphDialogTitleText = mIphDialogView.findViewById(R.id.title);
        TextView iphDialogDescriptionText = mIphDialogView.findViewById(R.id.description);
        mAnimationCallback = new Animatable2Compat.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                Handler handler = new Handler();
                handler.postDelayed(mIphAnimation::start, 1500);
            }
        };
        backgroundView.addView(mIphDialogView);
        mIphWindow = new PopupWindow(backgroundView, ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        mScrimView = new ScrimView(mContext, null, backgroundView);
        mDialogMarginParams = (ViewGroup.MarginLayoutParams) mIphDialogView.getLayoutParams();
        mTitleTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogTitleText.getLayoutParams();
        mDescriptionTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogDescriptionText.getLayoutParams();
        mCloseButtonMarginParams =
                (ViewGroup.MarginLayoutParams) mCloseIPHDialogButton.getLayoutParams();

        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                updateMargins(newConfig.orientation);
            }

            @Override
            public void onLowMemory() {}
        };
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /**
     * Setup the {@link View.OnClickListener} for close button in the IPH dialog.
     *
     * @param listener The {@link View.OnClickListener} used to setup close button in IPH dialog.
     */
    void setupCloseIphDialogButtonOnclickListener(View.OnClickListener listener) {
        mCloseIPHDialogButton.setOnClickListener(listener);
    }

    /**
     * Setup the {@link ScrimView.ScrimParams} for the {@link ScrimView}.
     *
     * @param observer The {@link ScrimView.ScrimObserver} used to setup the scrim view params.
     */
    void setupIPHDialogScrimViewObserver(ScrimView.ScrimObserver observer) {
        mScrimParams = new ScrimView.ScrimParams(mIphDialogView, false, true, 0, observer);
    }

    /**
     * Close the IPH dialog and hide the scrim view.
     */
    void closeIphDialog() {
        mIphWindow.dismiss();
        mScrimView.hideScrim(true);
        AnimatedVectorDrawableCompat.unregisterAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.stop();
    }

    /**
     * Show the scrim view and show dialog above it.
     */
    void showIPHDialog() {
        if (mScrimParams != null) {
            mScrimView.showScrim(mScrimParams);
        }
        updateMargins(mContext.getResources().getConfiguration().orientation);
        mIphWindow.showAtLocation(mParentView, Gravity.CENTER, 0, 0);
        AnimatedVectorDrawableCompat.registerAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.start();
    }

    private void updateMargins(int orientation) {
        int parentViewHeight = mParentView.getHeight();
        int dialogHeight =
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_height);
        // Dynamically setup the top margin base on parent view height, the minimum top margin is
        // specified in case the parent view height is smaller than or too close to dialog height.
        int updatedDialogTopMargin = Math.max((parentViewHeight - dialogHeight) / 2,
                (int) mContext.getResources().getDimension(R.dimen.tab_grid_iph_dialog_top_margin));

        int dialogTopMargin;
        int dialogSideMargin;
        int textTopMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            dialogTopMargin = updatedDialogTopMargin;
            dialogSideMargin = mDialogSideMargin;
            textTopMargin = mDialogTextTopMarginPortrait;
        } else {
            dialogTopMargin = mDialogSideMargin;
            dialogSideMargin = updatedDialogTopMargin;
            textTopMargin = mDialogTextTopMarginLandscape;
        }
        mDialogMarginParams.setMargins(
                dialogSideMargin, dialogTopMargin, dialogSideMargin, dialogTopMargin);
        mTitleTextMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
        mDescriptionTextMarginParams.setMargins(
                mDialogTextSideMargin, 0, mDialogTextSideMargin, textTopMargin);
        mCloseButtonMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    @VisibleForTesting
    PopupWindow getIphWindowForTesting() {
        return mIphWindow;
    }
}

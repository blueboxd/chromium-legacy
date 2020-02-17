// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.res.Resources;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v4.view.animation.LinearOutSlowInInterpolator;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

import java.util.LinkedList;
import java.util.List;

/**
 * The coordinator handling with the bottom toolbar transition between Browsing Mode and Tab
 * Switcher Mode. It does following changes:
 *  1. replace the icon of search accelerator.
 *  2. extend / shorten the width of search accelerator to match the width of new tab button.
 *  3. update the alpha values of home button, menu button and share button.
 */
public class BottomToolbarAnimationCoordinator extends EmptyOverviewModeObserver {
    private static final long DURATION =
            ToolbarManager.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;

    /** The browsing mode bottom toolbar component */
    private final BrowsingModeBottomToolbarCoordinator mBrowsingModeCoordinator;

    /** The tab switcher mode bottom toolbar component */
    private final TabSwitcherBottomToolbarCoordinator mTabSwitcherModeCoordinator;

    public BottomToolbarAnimationCoordinator(
            BrowsingModeBottomToolbarCoordinator browsingModeBottomToolbarCoordinator,
            TabSwitcherBottomToolbarCoordinator tabSwitcherBottomToolbarCoordinator) {
        mBrowsingModeCoordinator = browsingModeBottomToolbarCoordinator;
        mTabSwitcherModeCoordinator = tabSwitcherBottomToolbarCoordinator;
    }

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        startAnimation(false);
    }

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        startAnimation(true);
    }

    /**
     * Internal helper function to run animation.
     * @param showBrowsingMode Whether to start showing browsing mode.
     */
    private void startAnimation(boolean showBrowsingMode) {
        // disable toolbar to prevent from click events during transition
        mBrowsingModeCoordinator.setTouchEnabled(false);
        if (showBrowsingMode) {
            mBrowsingModeCoordinator.setVisible(true);
            mTabSwitcherModeCoordinator.setVisible(false);
        }

        float targetAlpha = showBrowsingMode ? 1.f : 0.f;
        List<Animator> animators = new LinkedList<>();
        animators.add(ObjectAnimator.ofFloat(
                mBrowsingModeCoordinator.getHomeButton(), View.ALPHA, targetAlpha));
        animators.add(ObjectAnimator.ofFloat(
                mBrowsingModeCoordinator.getShareButton(), View.ALPHA, targetAlpha));
        animators.add(ObjectAnimator.ofFloat(
                mBrowsingModeCoordinator.getTabSwitcherButtonView(), View.ALPHA, targetAlpha));

        View searchAccelerator = mBrowsingModeCoordinator.getSearchAccelerator();
        Resources res = searchAccelerator.getResources();
        int height = res.getDimensionPixelOffset(R.dimen.search_accelerator_height);
        int width = res.getDimensionPixelOffset(R.dimen.search_accelerator_width);
        int marginWidth = res.getDimensionPixelOffset(R.dimen.search_accelerator_width_margin);
        ValueAnimator widthAnimator = showBrowsingMode
                ? ValueAnimator.ofInt(height, width) // extend
                : ValueAnimator.ofInt(width, height); // shorten
        widthAnimator.addUpdateListener(animation -> {
            MarginLayoutParams params = (MarginLayoutParams) searchAccelerator.getLayoutParams();
            params.width = (int) animation.getAnimatedValue();
            // Update margin to prevent neighbour views from moving close to search accelerator.
            params.leftMargin = params.rightMargin =
                    marginWidth + (width - (int) animation.getAnimatedValue()) / 2;
            searchAccelerator.requestLayout();
        });
        mBrowsingModeCoordinator.getSearchAccelerator().setImageResource(
                showBrowsingMode ? R.drawable.ic_search : R.drawable.new_tab_icon);
        animators.add(widthAnimator);

        animators.get(0).addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (!showBrowsingMode) {
                    mBrowsingModeCoordinator.setVisible(false);
                    mTabSwitcherModeCoordinator.setVisible(true);
                }
                mBrowsingModeCoordinator.setTouchEnabled(true);
            }
        });

        for (Animator animator : animators) {
            animator.setInterpolator(showBrowsingMode ? new LinearOutSlowInInterpolator()
                                                      : new FastOutLinearInInterpolator());
            animator.setDuration(DURATION).start();
        }
    }
}

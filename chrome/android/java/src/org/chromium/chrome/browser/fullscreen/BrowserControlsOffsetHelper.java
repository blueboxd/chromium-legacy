// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * Handles browser controls offset.
 */
public class BrowserControlsOffsetHelper implements VrModeObserver, Destroyable {
    /**
     * Maximum duration for the control container slide-in animation. Note that this value matches
     * the one in browser_controls_offset_manager.cc.
     */
    private static final int MAX_CONTROLS_ANIMATION_DURATION_MS = 200;

    /** Observes active tab. */
    private final ActivityTabTabObserver mTabObserver;

    /* Provides {@link FullscreenManager} lazily when actually used. */
    private final Supplier<FullscreenManager> mFullscreenManager;

    /** Active tab in the foreground. */
    private Tab mTab;

    /** The animator for slide-in animation on the Android controls. */
    private ValueAnimator mControlsAnimator;

    /** Whether the browser is currently in VR mode. */
    private boolean mIsInVr;

    /**
     * Indicates if control offset is in the overridden state by animation. Stays {@code true}
     * from animation start till the next offset update from compositor arrives.
     */
    private boolean mOffsetOverridden;

    public BrowserControlsOffsetHelper(ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider activityTabProvider,
            Supplier<FullscreenManager> fullscreenManager) {
        mFullscreenManager = fullscreenManager;
        mTab = activityTabProvider.get();
        mTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            protected void onObservingDifferentTab(Tab tab) {
                // Got into a state where there's no active tab (e.g. tab switcher). Wait till the
                // next non-null active tab is set for any update.
                if (tab == null) return;
                mTab = tab;
                restorePositions();
            }

            @Override
            public void onCrash(Tab tab) {
                if (SadTab.isShowing(tab)) showAndroidControls(false);
            }

            @Override
            public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                if (!isResponsive) showAndroidControls(false);
            }

            @Override
            public void onBrowserControlsOffsetChanged(
                    int topControlsOffset, int bottomControlsOffset, int contentOffset) {
                onOffsetsChanged(topControlsOffset, bottomControlsOffset, contentOffset);
            }
        };

        VrModuleProvider.registerVrModeObserver(this);
        if (VrModuleProvider.getDelegate().isInVr()) onEnterVr();
        lifecycleDispatcher.register(this);
    }

    /**
     * Called when offset values related with fullscreen functionality has been changed by the
     * compositor.
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param contentOffsetY The Y offset of the content in physical pixels.
     */
    private void onOffsetsChanged(
            int topControlsOffsetY, int bottomControlsOffsetY, int contentOffsetY) {
        // Cancel any animation on the Android controls and let compositor drive the offset updates.
        resetControlsOffsetOverridden();

        if (SadTab.isShowing(mTab) || mTab.isNativePage()) {
            showAndroidControls(false);
        } else {
            updateFullscreenManagerOffsets(
                    false, topControlsOffsetY, bottomControlsOffsetY, contentOffsetY);
        }
        TabModelImpl.setActualTabSwitchLatencyMetricRequired();
    }

    /**
     * Shows the Android browser controls view.
     * @param animate Whether a slide-in animation should be run.
     */
    public void showAndroidControls(boolean animate) {
        if (animate) {
            runBrowserDrivenShowAnimation();
        } else {
            updateFullscreenManagerOffsets(true, 0, 0, mFullscreenManager.get().getContentOffset());
        }
    }

    /**
     * Restores the controls positions to the cached positions of the active Tab.
     */
    private void restorePositions() {
        resetControlsOffsetOverridden();

        // Make sure the dominant control offsets have been set.
        TabBrowserControlsState controlState = TabBrowserControlsState.get(mTab);
        if (controlState.offsetInitialized()) {
            updateFullscreenManagerOffsets(false, controlState.topControlsOffset(),
                    controlState.bottomControlsOffset(), controlState.contentOffset());
        } else {
            showAndroidControls(false);
        }
        TabBrowserControlsState.updateEnabledState(mTab);
    }

    /**
     * Helper method to update offsets in {@link FullscreenManager} and notify offset changes to
     * observers if necessary.
     */
    private void updateFullscreenManagerOffsets(boolean toNonFullscreen, int topControlsOffset,
            int bottomControlsOffset, int topContentOffset) {
        FullscreenManager manager = mFullscreenManager.get();
        if (mIsInVr) {
            VrModuleProvider.getDelegate().rawTopContentOffsetChanged(topContentOffset);
            // The dip scale of java UI and WebContents are different while in VR, leading to a
            // mismatch in size in pixels when converting from dips. Since we hide the controls in
            // VR anyways, just set the offsets to what they're supposed to be with the controls
            // hidden.
            // TODO(mthiesse): Should we instead just set the top controls height to be 0 while in
            // VR?
            topControlsOffset = -manager.getTopControlsHeight();
            bottomControlsOffset = manager.getBottomControlsHeight();
            topContentOffset = 0;
            manager.setPositionsForTab(topControlsOffset, bottomControlsOffset, topContentOffset);
        } else if (toNonFullscreen) {
            manager.setPositionsForTabToNonFullscreen();
        } else {
            manager.setPositionsForTab(topControlsOffset, bottomControlsOffset, topContentOffset);
        }
    }

    /** @return {@code true} if browser control offset is overridden by animation. */
    public boolean offsetOverridden() {
        return mOffsetOverridden;
    }

    /**
     * Sets the flat indicating if browser control offset is overridden by animation.
     * @param flag Boolean flag of the new offset overridden state.
     */
    private void setOffsetOverridden(boolean flag) {
        mOffsetOverridden = flag;
    }

    /**
     * Helper method to cancel overridden offset on Android browser controls.
     */
    private void resetControlsOffsetOverridden() {
        if (!offsetOverridden()) return;
        if (mControlsAnimator != null) mControlsAnimator.cancel();
        setOffsetOverridden(false);
    }

    /**
     * Helper method to run slide-in animations on the Android browser controls views.
     */
    private void runBrowserDrivenShowAnimation() {
        if (mControlsAnimator != null) return;

        TabBrowserControlsState controlState = TabBrowserControlsState.get(mTab);
        setOffsetOverridden(true);

        final FullscreenManager manager = mFullscreenManager.get();
        final float hiddenRatio = manager.getBrowserControlHiddenRatio();
        final int topControlHeight = manager.getTopControlsHeight();
        final int topControlOffset = manager.getTopControlOffset();

        // Set animation start value to current renderer controls offset.
        mControlsAnimator = ValueAnimator.ofInt(topControlOffset, 0);
        mControlsAnimator.setDuration(
                (long) Math.abs(hiddenRatio * MAX_CONTROLS_ANIMATION_DURATION_MS));
        mControlsAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mControlsAnimator = null;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                updateFullscreenManagerOffsets(false, topControlHeight, 0, topControlHeight);
            }
        });
        mControlsAnimator.addUpdateListener((animator) -> {
            updateFullscreenManagerOffsets(
                    false, (int) animator.getAnimatedValue(), 0, topControlHeight);
        });
        mControlsAnimator.start();
    }

    @Override
    public void onEnterVr() {
        mIsInVr = true;
        restorePositions();
    }

    @Override
    public void onExitVr() {
        mIsInVr = false;

        // Clear the VR-specific overrides for controls height.
        restorePositions();

        // Show the Controls explicitly because under some situations, like when we're showing a
        // Native Page, the renderer won't send any new offsets.
        showAndroidControls(false);
    }

    @Override
    public void destroy() {
        VrModuleProvider.unregisterVrModeObserver(this);
        mTabObserver.destroy();
    }
}

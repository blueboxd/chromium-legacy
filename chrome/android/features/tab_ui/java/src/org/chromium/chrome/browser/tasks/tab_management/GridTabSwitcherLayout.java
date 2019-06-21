// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.compositor.animation.CompositorAnimator.FAST_OUT_SLOW_IN_INTERPOLATOR;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Locale;

/**
 * A {@link Layout} that shows all tabs in one grid view.
 */
public class GridTabSwitcherLayout
        extends Layout implements GridTabSwitcher.GridVisibilityObserver {
    private static final String TAG = "GTSLayout";

    // Duration of the transition animation
    @VisibleForTesting
    static final long ZOOMING_DURATION = 300;
    private static final int BACKGROUND_FADING_DURATION_MS = 150;

    /** Field trial parameter for whether skipping slow zooming animation */
    private static final String SKIP_SLOW_ZOOMING_PARAM = "skip-slow-zooming";
    private static final boolean DEFAULT_SKIP_SLOW_ZOOMING = false;

    // The transition animation from a tab to the tab switcher.
    private AnimatorSet mTabToSwitcherAnimation;

    private final TabListSceneLayer mSceneLayer = new TabListSceneLayer();
    private final GridTabSwitcher mGridTabSwitcher;
    private final GridTabSwitcher.GridController mGridController;
    // To force Toolbar finishes its animation when this Layout finished hiding.
    private final LayoutTab mDummyLayoutTab;

    private float mBackgroundAlpha;

    private int mFrameCount;
    private long mStartTime;
    private long mLastFrameTime;
    private long mMaxFrameInterval;
    private int mStartFrame;

    interface PerfListener {
        void onAnimationDone(
                int frameRendered, long elapsedMs, long maxFrameInterval, int dirtySpan);
    }

    private PerfListener mPerfListenerForTesting;

    public GridTabSwitcherLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, GridTabSwitcher gridTabSwitcher) {
        super(context, updateHost, renderHost);
        mDummyLayoutTab = createLayoutTab(Tab.INVALID_TAB_ID, false, false, false);
        mDummyLayoutTab.setShowToolbar(true);
        mGridTabSwitcher = gridTabSwitcher;
        mGridController = gridTabSwitcher.getGridController();
        mGridController.addOverviewModeObserver(this);
    }

    @Override
    public LayoutTab getLayoutTab(int id) {
        return mDummyLayoutTab;
    }

    @Override
    public void destroy() {
        if (mGridController != null) {
            mGridController.removeOverviewModeObserver(this);
        }
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        boolean showShrinkingAnimation =
                animate && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION);
        boolean quick = mGridTabSwitcher.prepareOverview();
        if (getSkipSlowZooming()) {
            showShrinkingAnimation &= quick;
        }

        if (!showShrinkingAnimation) {
            // Keep the current tab in mLayoutTabs so that thumbnail taking is not blocked.
            LayoutTab sourceLayoutTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                    mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NEED_TITLE);
            mLayoutTabs = new LayoutTab[] {sourceLayoutTab};

            mGridController.showOverview(animate);
            return;
        }

        shrinkTab(() -> mGridTabSwitcher.getThumbnailLocationOfCurrentTab(false));
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        if (mLayoutTabs == null) return;

        assert mLayoutTabs.length == 1;
        boolean needUpdate = mLayoutTabs[0].updateSnap(dt);
        if (needUpdate) requestUpdate();
    }

    @Override
    public void doneHiding() {
        super.doneHiding();
        RecordUserAction.record("MobileExitStackView");
    }

    @Override
    public boolean onBackPressed() {
        if (mTabModelSelector.getCurrentModel().getCount() == 0) return false;
        return mGridController.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesCloseAll() {
        return false;
    }

    // GridTabSwitcher.GridVisibilityObserver implementation.
    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {}

    @Override
    public void onOverviewModeFinishedShowing() {
        doneShowing();
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab != null) mTabContentManager.cacheTabThumbnail(currentTab);
    }

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        startHiding(mTabModelSelector.getCurrentTabId(), false);
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)) {
            mGridTabSwitcher.postHiding();
            return;
        }
        expandTab(mGridTabSwitcher.getThumbnailLocationOfCurrentTab(true));
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    @Override
    protected void forceAnimationToFinish() {
        super.forceAnimationToFinish();
        if (mTabToSwitcherAnimation != null) {
            if (mTabToSwitcherAnimation.isRunning()) mTabToSwitcherAnimation.end();
        }
    }

    /**
     * Animate shrinking a tab to a target {@link Rect} area.
     * @param target The target {@link Rect} area.
     */
    private void shrinkTab(Supplier<Rect> target) {
        LayoutTab sourceLayoutTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NEED_TITLE);
        sourceLayoutTab.setDecorationAlpha(0);

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};

        forceAnimationToFinish();

        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Step 1: zoom out the source tab
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.SCALE, () -> 1f, () -> {
                    return target.get().width() / (getWidth() * mDpToPx);
                }, ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.X, () -> 0f, () -> {
                    return target.get().left / mDpToPx;
                }, ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.Y, () -> 0f, () -> {
                    return target.get().top / mDpToPx;
                }, ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, sourceLayoutTab.getUnclampedOriginalContentHeight(),
                getWidth(), ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 0f, 1f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(CompositorAnimator.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                // Step 2: fade in the real GTS RecyclerView.
                mGridController.showOverview(true);

                reportAnimationPerf();
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    /**
     * Animate expanding a tab from a source {@link Rect} area.
     * @param source The source {@link Rect} area.
     */
    private void expandTab(Rect source) {
        LayoutTab sourceLayoutTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NEED_TITLE);
        sourceLayoutTab.setDecorationAlpha(0);

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};

        forceAnimationToFinish();

        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Zoom in the source tab
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.SCALE, source.width() / (getWidth() * mDpToPx), 1, ZOOMING_DURATION,
                FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.X,
                source.left / mDpToPx, 0f, ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.Y,
                source.top / mDpToPx, 0f, ZOOMING_DURATION, FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, getWidth(),
                sourceLayoutTab.getUnclampedOriginalContentHeight(), ZOOMING_DURATION,
                FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 1f, 0f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(CompositorAnimator.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                mGridTabSwitcher.postHiding();

                reportAnimationPerf();
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    void setPerfListenerForTesting(PerfListener perfListener) {
        mPerfListenerForTesting = perfListener;
    }

    @VisibleForTesting
    GridTabSwitcher getGridTabSwitcherForTesting() {
        return mGridTabSwitcher;
    }

    private void reportAnimationPerf() {
        int frameRendered = mFrameCount - mStartFrame;
        long elapsedMs = SystemClock.elapsedRealtime() - mStartTime;
        long lastDirty = mGridTabSwitcher.getLastDirtyTimeForTesting();
        int dirtySpan = (int) (lastDirty - mStartTime);
        float fps = 1000.f * frameRendered / elapsedMs;
        String message = String.format(Locale.US,
                "fps = %.2f (%d / %dms), maxFrameInterval = %d, dirtySpan = %d", fps, frameRendered,
                elapsedMs, mMaxFrameInterval, dirtySpan);

        // TODO(crbug.com/964406): stop reporting on Canary before enabling in Finch.
        if (ChromeVersionInfo.isLocalBuild() || ChromeVersionInfo.isCanaryBuild()) {
            Toast.makeText(ContextUtils.getApplicationContext(), message, Toast.LENGTH_SHORT)
                    .show();
        }

        // TODO(crbug.com/964406): stop logging it after this feature stabilizes.
        if (!ChromeVersionInfo.isStableBuild()) {
            Log.i(TAG, message);
        }

        if (mPerfListenerForTesting != null) {
            mPerfListenerForTesting.onAnimationDone(
                    frameRendered, elapsedMs, mMaxFrameInterval, dirtySpan);
        }
    }

    private boolean getSkipSlowZooming() {
        String skip = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, SKIP_SLOW_ZOOMING_PARAM);
        try {
            return Boolean.valueOf(skip);
        } catch (NumberFormatException e) {
            return DEFAULT_SKIP_SLOW_ZOOMING;
        }
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mSceneLayer != null;
        // The content viewport is intentionally sent as both params below.
        mSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                layerTitleCache, tabContentManager, resourceManager, fullscreenManager,
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                        ? mGridTabSwitcher.getResourceId()
                        : 0,
                mBackgroundAlpha);
        mFrameCount++;
        if (mLastFrameTime != 0) {
            long elapsed = SystemClock.elapsedRealtime() - mLastFrameTime;
            mMaxFrameInterval = Math.max(mMaxFrameInterval, elapsed);
        }
        mLastFrameTime = SystemClock.elapsedRealtime();
    }
}

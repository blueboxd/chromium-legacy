// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ApplicationStatus.WindowFocusChangedListener;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabBrowserControlsOffsetHelper;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.vr.VrModeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * A class that manages control and content views to create the fullscreen mode.
 */
public class ChromeFullscreenManager implements ActivityStateListener, WindowFocusChangedListener,
                                                ViewGroup.OnHierarchyChangeListener,
                                                View.OnSystemUiVisibilityChangeListener,
                                                VrModeObserver, BrowserControlsStateProvider {
    // The amount of time to delay the control show request after returning to a once visible
    // activity.  This delay is meant to allow Android to run its Activity focusing animation and
    // have the controls scroll back in smoothly once that has finished.
    private static final long ACTIVITY_RETURN_SHOW_REQUEST_DELAY_MS = 100;

    /**
     * Maximum duration for the control container slide-in animation. Note that this value matches
     * the one in browser_controls_offset_manager.cc.
     */
    private static final int MAX_CONTROLS_ANIMATION_DURATION_MS = 200;

    private final Activity mActivity;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    @ControlsPosition private final int mControlsPosition;
    private final boolean mExitFullscreenOnStop;
    private final TokenHolder mHidingTokenHolder = new TokenHolder(this::scheduleVisibilityUpdate);

    /**
     * An observable for browser controls being at its minimum height or not.
     * This is as good as the controls being hidden when both min heights are 0.
     */
    private final ObservableSupplierImpl<Boolean> mControlsAtMinHeight =
            new ObservableSupplierImpl<>();

    private TabModelSelectorTabObserver mTabFullscreenObserver;
    @Nullable private ControlContainer mControlContainer;
    private int mTopControlContainerHeight;
    private int mTopControlsMinHeight;
    private int mBottomControlContainerHeight;
    private int mBottomControlsMinHeight;
    private boolean mAnimateBrowserControlsHeightChanges;
    private boolean mControlsResizeView;

    private int mRendererTopControlOffset;
    private int mRendererBottomControlOffset;
    private int mRendererTopContentOffset;
    private int mRendererTopControlsMinHeightOffset;
    private int mRendererBottomControlsMinHeightOffset;
    private int mPreviousContentOffset;
    private float mControlOffsetRatio;
    private boolean mOffsetsChanged;
    private ActivityTabTabObserver mActiveTabObserver;

    private boolean mInGesture;
    private boolean mContentViewScrolling;

    // Current ContentView. Updates when active tab is switched or WebContents is swapped
    // in the current Tab.
    private ContentView mContentView;

    private final ArrayList<FullscreenListener> mListeners = new ArrayList<>();
    private final ObserverList<BrowserControlsStateProvider.Observer> mControlsObservers =
            new ObserverList<>();
    private Runnable mViewportSizeDelegate;
    private FullscreenHtmlApiHandler mHtmlApiHandler;
    @Nullable
    private Tab mTab;

    /** The animator for slide-in animation on the Android controls. */
    private ValueAnimator mControlsAnimator;

    /**
     * Indicates if control offset is in the overridden state by animation. Stays {@code true}
     * from animation start till the next offset update from compositor arrives.
     */
    private boolean mOffsetOverridden;

    @IntDef({ControlsPosition.TOP, ControlsPosition.NONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ControlsPosition {
        /** Controls are at the top, eg normal ChromeTabbedActivity. */
        int TOP = 0;
        /** Controls are not present, eg NoTouchActivity. */
        int NONE = 1;
    }

    /**
     * A listener that gets notified of changes to the fullscreen state.
     */
    public interface FullscreenListener {
        /**
         * Called when entering fullscreen mode.
         * @param tab The tab whose content is entering fullscreen mode.
         * @param options Options to adjust fullscreen mode.
         */
        default void onEnterFullscreen(Tab tab, FullscreenOptions options) {}

        /**
         * Called when exiting fullscreen mode.
         * @param tab The tab whose content is exiting fullscreen mode.
         */
        default void onExitFullscreen(Tab tab) {}
    }

    private final Runnable mUpdateVisibilityRunnable = new Runnable() {
        @Override
        public void run() {
            int visibility = shouldShowAndroidControls() ? View.VISIBLE : View.INVISIBLE;
            if (mControlContainer == null
                    || mControlContainer.getView().getVisibility() == visibility) {
                return;
            }
            // requestLayout is required to trigger a new gatherTransparentRegion(), which
            // only occurs together with a layout and let's SurfaceFlinger trim overlays.
            // This may be almost equivalent to using View.GONE, but we still use View.INVISIBLE
            // since drawing caches etc. won't be destroyed, and the layout may be less expensive.
            mControlContainer.getView().setVisibility(visibility);
            mControlContainer.getView().requestLayout();
        }
    };

    /**
     * Creates an instance of the fullscreen mode manager.
     * @param activity The activity that supports fullscreen.
     * @param controlsPosition Where the browser controls are.
     */
    public ChromeFullscreenManager(Activity activity, @ControlsPosition int controlsPosition) {
        this(activity, controlsPosition, true);
    }

    /**
     * Creates an instance of the fullscreen mode manager.
     * @param activity The activity that supports fullscreen.
     * @param controlsPosition Where the browser controls are.
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be
     *                             true for Activities that are not always fullscreen.
     */
    public ChromeFullscreenManager(Activity activity,
            @ControlsPosition int controlsPosition, boolean exitFullscreenOnStop) {
        mActivity = activity;
        mControlsPosition = controlsPosition;
        mExitFullscreenOnStop = exitFullscreenOnStop;
        mControlsAtMinHeight.set(false);
        mHtmlApiHandler = new FullscreenHtmlApiHandler(activity.getWindow(), this::getTab,
                mControlsAtMinHeight, () -> !(isInVr() || bootsToVr()));
        mBrowserVisibilityDelegate = new BrowserStateBrowserControlsVisibilityDelegate(
                mHtmlApiHandler.getPersistentFullscreenModeSupplier());
        mBrowserVisibilityDelegate.addObserver((constraints) -> {
            if (constraints == BrowserControlsState.SHOWN) setPositionsForTabToNonFullscreen();
        });
        VrModuleProvider.registerVrModeObserver(this);
        if (isInVr()) onEnterVr();
    }

    /**
     * Initializes the fullscreen manager with the required dependencies.
     *
     * @param controlContainer Container holding the controls (Toolbar).
     * @param activityTabProvider Provider of the current activity tab.
     * @param modelSelector The tab model selector that will be monitored for tab changes.
     * @param resControlContainerHeight The dimension resource ID for the control container height.
     */
    public void initialize(ControlContainer controlContainer,
            ActivityTabProvider activityTabProvider, final TabModelSelector modelSelector,
            int resControlContainerHeight) {
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        ApplicationStatus.registerWindowFocusChangedListener(this);
        mActiveTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            protected void onObservingDifferentTab(Tab tab) {
                setTab(tab);
            }

            @Override
            public void onContentViewScrollingStateChanged(boolean scrolling) {
                mContentViewScrolling = scrolling;
                if (!scrolling) updateContentOffsetAndNotify();
            }
        };

        mTabFullscreenObserver = new TabModelSelectorTabObserver(modelSelector) {
            @Override
            public void onHidden(Tab tab, @TabHidingType int reason) {
                // Clean up any fullscreen state that might impact other tabs.
                exitPersistentFullscreenMode();
            }

            @Override
            public void onContentChanged(Tab tab) {
                if (tab == getTab()) updateViewStateListener();
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.isInMainFrame() && !navigation.isSameDocument()) {
                    if (tab == getTab()) exitPersistentFullscreenMode();
                }
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean interactable) {
                Tab currentTab = getTab();
                if (!interactable || tab != currentTab) return;
                Runnable enterFullscreen = getEnterFullscreenRunnable(currentTab);
                if (enterFullscreen != null) enterFullscreen.run();

                TabBrowserControlsOffsetHelper offsetHelper =
                        TabBrowserControlsOffsetHelper.get(currentTab);
                if (!offsetHelper.offsetInitialized()) return;

                onOffsetsChanged(offsetHelper.topControlsOffset(),
                        offsetHelper.bottomControlsOffset(), offsetHelper.contentOffset(),
                        offsetHelper.topControlsMinHeightOffset(),
                        offsetHelper.bottomControlsMinHeightOffset());
            }

            @Override
            public void onCrash(Tab tab) {
                if (tab == getTab() && SadTab.isShowing(tab)) showAndroidControls(false);
            }

            @Override
            public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                if (tab == getTab() && !isResponsive) showAndroidControls(false);
            }

            @Override
            public void onBrowserControlsOffsetChanged(Tab tab, int topControlsOffset,
                    int bottomControlsOffset, int contentOffset, int topControlsMinHeightOffset,
                    int bottomControlsMinHeightOffset) {
                if (tab == getTab() && tab.isUserInteractable()) {
                    onOffsetsChanged(topControlsOffset, bottomControlsOffset, contentOffset,
                            topControlsMinHeightOffset, bottomControlsMinHeightOffset);
                }
            }
        };
        assert controlContainer != null || mControlsPosition == ControlsPosition.NONE;
        mControlContainer = controlContainer;

        switch (mControlsPosition) {
            case ControlsPosition.TOP:
                assert resControlContainerHeight != ChromeActivity.NO_CONTROL_CONTAINER;
                mTopControlContainerHeight =
                        mActivity.getResources().getDimensionPixelSize(resControlContainerHeight);
                break;
            case ControlsPosition.NONE:
                // Treat the case of no controls as controls always being totally offscreen.
                mControlOffsetRatio = 1.0f;
                break;
        }

        mRendererTopContentOffset = mTopControlContainerHeight;
        updateControlOffset();
        scheduleVisibilityUpdate();
    }

    /**
     * @return The visibility delegate that allows browser UI to control the browser control
     *         visibility.
     */
    public BrowserStateBrowserControlsVisibilityDelegate getBrowserVisibilityDelegate() {
        return mBrowserVisibilityDelegate;
    }

    /**
     * @return The currently selected tab for fullscreen.
     */
    @Nullable
    @VisibleForTesting
    Tab getTab() {
        return mTab;
    }

    private void setTab(@Nullable Tab tab) {
        Tab previousTab = getTab();
        mTab = tab;
        if (previousTab != tab) {
            updateViewStateListener();
            if (tab != null) {
                mBrowserVisibilityDelegate.showControlsTransient();
                updateMultiTouchZoomSupport(!getPersistentFullscreenMode());
                if (tab.isUserInteractable()) restoreControlsPositions();
            }
        }

        if (tab == null && mBrowserVisibilityDelegate.get() != BrowserControlsState.HIDDEN) {
            setPositionsForTabToNonFullscreen();
        }
    }

    /**
     * Enter fullscreen.
     * @param tab {@link Tab} that goes into fullscreen.
     * @param options Fullscreen options.
     */
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        // If enabling fullscreen while the tab is not interactable, fullscreen
        // will be delayed until the tab is interactable.
        Runnable r = () -> {
            enterPersistentFullscreenMode(options);
            destroySelectActionMode(tab);
            setEnterFullscreenRunnable(tab, null);
        };

        if (tab.isUserInteractable()) {
            r.run();
        } else {
            setEnterFullscreenRunnable(tab, r);
        }

        for (FullscreenListener listener : mListeners) listener.onEnterFullscreen(tab, options);
    }

    /**
     * Exit fullscreen.
     * @param tab {@link Tab} that goes out of fullscreen.
     */
    public void onExitFullscreen(Tab tab) {
        setEnterFullscreenRunnable(tab, null);
        if (tab == getTab()) exitPersistentFullscreenMode();
        for (FullscreenListener listener : mListeners) listener.onExitFullscreen(tab);
    }

    /**
     * Enters persistent fullscreen mode.  In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     */
    private void enterPersistentFullscreenMode(FullscreenOptions options) {
        mHtmlApiHandler.enterPersistentFullscreenMode(options);
        updateMultiTouchZoomSupport(false);
    }

    /**
     * Exits persistent fullscreen mode.  In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     */
    public void exitPersistentFullscreenMode() {
        mHtmlApiHandler.exitPersistentFullscreenMode();
        updateMultiTouchZoomSupport(true);
    }

    /**
     * @see GestureListenerManager#updateMultiTouchZoomSupport(boolean).
     */
    private void updateMultiTouchZoomSupport(boolean enable) {
        Tab tab = getTab();
        if (tab == null || tab.isHidden()) return;
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
            if (manager != null) manager.updateMultiTouchZoomSupport(enable);
        }
    }

    /**
     * @return Whether the application is in persistent fullscreen mode.
     * @see #setPersistentFullscreenMode(boolean)
     */
    public boolean getPersistentFullscreenMode() {
        return mHtmlApiHandler.getPersistentFullscreenMode();
    }

    /**
     * Notified when the system UI visibility for the current ContentView has changed.
     * @param visibility The updated UI visibility.
     * @see View#getSystemUiVisibility()
     */
    public void onContentViewSystemUiVisibilityChange(int visibility) {
        mHtmlApiHandler.onContentViewSystemUiVisibilityChange(visibility);
    }

    private void destroySelectActionMode(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            SelectionPopupController.fromWebContents(webContents).destroySelectActionMode();
        }
    }

    private void setEnterFullscreenRunnable(Tab tab, Runnable runnable) {
        TabAttributes attrs = TabAttributes.from(tab);
        if (runnable == null) {
            attrs.clear(TabAttributeKeys.ENTER_FULLSCREEN);
        } else {
            attrs.set(TabAttributeKeys.ENTER_FULLSCREEN, runnable);
        }
    }

    private Runnable getEnterFullscreenRunnable(Tab tab) {
        return tab != null ? TabAttributes.from(tab).get(TabAttributeKeys.ENTER_FULLSCREEN) : null;
    }

    private void updateViewStateListener() {
        if (mContentView != null) {
            mContentView.removeOnHierarchyChangeListener(this);
            mContentView.removeOnSystemUiVisibilityChangeListener(this);
        }
        mContentView = getContentView();
        if (mContentView != null) {
            mContentView.addOnHierarchyChangeListener(this);
            mContentView.addOnSystemUiVisibilityChangeListener(this);
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STOPPED && mExitFullscreenOnStop) {
            // Exit fullscreen in onStop to ensure the system UI flags are set correctly when
            // showing again (on JB MR2+ builds, the omnibox would be covered by the
            // notification bar when this was done in onStart()).
            exitPersistentFullscreenMode();
        } else if (newState == ActivityState.STARTED) {
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                    ()
                            -> mBrowserVisibilityDelegate.showControlsTransient(),
                    ACTIVITY_RETURN_SHOW_REQUEST_DELAY_MS);
        } else if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterActivityStateListener(this);
            ApplicationStatus.unregisterWindowFocusChangedListener(this);
        }
    }

    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasFocus) {
        if (mActivity != activity) return;
        mHtmlApiHandler.onWindowFocusChanged(hasFocus);
        // {@link ContentVideoView#getContentVideoView} requires native to have been initialized.
        if (!LibraryLoader.getInstance().isInitialized()) return;
    }

    @Override
    public float getBrowserControlHiddenRatio() {
        return mControlOffsetRatio;
    }

    /**
     * @return True if the browser controls are completely off screen.
     */
    public boolean areBrowserControlsOffScreen() {
        return getBrowserControlHiddenRatio() == 1.0f;
    }

    /**
     * @return True if the browser controls are showing as much as the min height. Note that this is
     * the same as {@link #areBrowserControlsOffScreen()} when both min-heights are 0.
     */
    @VisibleForTesting
    boolean areBrowserControlsAtMinHeight() {
        return mControlsAtMinHeight.get();
    }

    /**
     * @return True if the browser controls are currently completely visible.
     */
    public boolean areBrowserControlsFullyVisible() {
        return getBrowserControlHiddenRatio() == 0.f;
    }

    /**
     * @return Whether the browser controls should be drawn as a texture.
     */
    public boolean drawControlsAsTexture() {
        return getBrowserControlHiddenRatio() > 0;
    }

    /**
     * Sets the height of the bottom controls.
     */
    public void setBottomControlsHeight(int bottomControlsHeight, int bottomControlsMinHeight) {
        if (mBottomControlContainerHeight == bottomControlsHeight
                && mBottomControlsMinHeight == bottomControlsMinHeight) {
            return;
        }
        mBottomControlContainerHeight = bottomControlsHeight;
        mBottomControlsMinHeight = bottomControlsMinHeight;
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onBottomControlsHeightChanged(
                    mBottomControlContainerHeight, mBottomControlsMinHeight);
        }
    }

    /**
     * Sets the height of the top controls.
     */
    public void setTopControlsHeight(int topControlsHeight, int topControlsMinHeight) {
        if (mTopControlContainerHeight == topControlsHeight
                && mTopControlsMinHeight == topControlsMinHeight) {
            return;
        }
        mTopControlContainerHeight = topControlsHeight;
        mTopControlsMinHeight = topControlsMinHeight;
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onTopControlsHeightChanged(mTopControlContainerHeight, mTopControlsMinHeight);
        }
    }

    /**
     * Sets whether the changes to the browser controls heights should be animated.
     * @param animateBrowserControlsHeightChanges True if the height changes should be animated.
     */
    public void setAnimateBrowserControlsHeightChanges(
            boolean animateBrowserControlsHeightChanges) {
        mAnimateBrowserControlsHeightChanges = animateBrowserControlsHeightChanges;
    }

    @Override
    public int getTopControlsHeight() {
        return mTopControlContainerHeight;
    }

    @Override
    public int getTopControlsMinHeight() {
        return mTopControlsMinHeight;
    }

    @Override
    public int getBottomControlsHeight() {
        return mBottomControlContainerHeight;
    }

    @Override
    public int getBottomControlsMinHeight() {
        return mBottomControlsMinHeight;
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return mAnimateBrowserControlsHeightChanges;
    }

    public boolean controlsResizeView() {
        return mControlsResizeView;
    }

    @Override
    public int getContentOffset() {
        return mRendererTopContentOffset;
    }

    @Override
    public int getTopControlOffset() {
        return mRendererTopControlOffset;
    }

    @Override
    public int getTopControlsMinHeightOffset() {
        return mRendererTopControlsMinHeightOffset;
    }

    /**
     * @return The content offset from the bottom of the screen, or the visible height of the bottom
     *         controls, in px.
     */
    public int getBottomContentOffset() {
        return getBottomControlsHeight() - getBottomControlOffset();
    }

    @Override
    public int getBottomControlOffset() {
        // If the height is currently 0, the offset generated by the bottom controls should be too.
        // TODO(crbug.com/103602): Send a offset update from the fullscreen manager when the height
        //                         changes to ensure correct offsets (removing the need for min()).
        return Math.min(mRendererBottomControlOffset, mBottomControlContainerHeight);
    }

    @Override
    public int getBottomControlsMinHeightOffset() {
        return mRendererBottomControlsMinHeightOffset;
    }

    private void updateControlOffset() {
        if (mControlsPosition == ControlsPosition.NONE) return;

        if (getTopControlsHeight() == 0) {
            // Treat the case of 0 height as controls being totally offscreen.
            mControlOffsetRatio = 1.0f;
        } else {
            mControlOffsetRatio =
                    Math.abs((float) mRendererTopControlOffset / getTopControlsHeight());
        }
    }

    @Override
    public float getTopVisibleContentOffset() {
        return getTopControlsHeight() + getTopControlOffset();
    }

    @Override
    public void addObserver(Observer obs) {
        mControlsObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(Observer obs) {
        mControlsObservers.removeObserver(obs);
    }

    /**
     * @param listener The {@link FullscreenListener} to be notified of fullscreen changes.
     */
    public void addListener(FullscreenListener listener) {
        if (!mListeners.contains(listener)) mListeners.add(listener);
    }

    /**
     * @param listener The {@link FullscreenListener} to no longer be notified of fullscreen
     *                 changes.
     */
    public void removeListener(FullscreenListener listener) {
        mListeners.remove(listener);
    }

    /**
     * @param delegate A Runnable to be executed when the WebContents viewport should be updated.
     */
    public void setViewportSizeDelegate(Runnable delegate) {
        mViewportSizeDelegate = delegate;
    }

    /**
     * Updates viewport size to have it render the content correctly.
     */
    public void updateViewportSize() {
        if (mInGesture || mContentViewScrolling) return;

        // Update content viewport size only if the browser controls are not moving, i.e. not
        // scrolling or animating.
        if (!areBrowserControlsIdle()) return;

        mControlsResizeView = getContentOffset() > getTopControlsMinHeight()
                || getBottomContentOffset() > getBottomControlsMinHeight();
        if (mViewportSizeDelegate != null) mViewportSizeDelegate.run();
    }

    /**
     * @return Whether browser controls are currently idle, i.e. not scrolling or animating.
     */
    private boolean areBrowserControlsIdle() {
        return (getContentOffset() == getTopControlsMinHeight()
                       || getContentOffset() == getTopControlsHeight())
                && (getBottomContentOffset() == getBottomControlsMinHeight()
                        || getBottomContentOffset() == getBottomControlsHeight());
    }

    // View.OnHierarchyChangeListener implementation

    @Override
    public void onChildViewRemoved(View parent, View child) {
        updateContentViewChildrenState();
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        updateContentViewChildrenState();
    }

    /**
     * Updates the current ContentView's children and any popups with the correct offsets based on
     * the current fullscreen state.
     */
    private void updateContentViewChildrenState() {
        ViewGroup view = getContentView();
        if (view == null) return;

        float topViewsTranslation = getTopVisibleContentOffset();
        float bottomMargin = getBottomContentOffset();
        applyTranslationToTopChildViews(view, topViewsTranslation);
        applyMarginToFullChildViews(view, topViewsTranslation, bottomMargin);
        updateViewportSize();
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        onContentViewSystemUiVisibilityChange(visibility);
    }

    /**
     * Utility routine for ensuring visibility updates are synchronized with
     * animation, preventing message loop stalls due to untimely invalidation.
     */
    private void scheduleVisibilityUpdate() {
        if (mControlContainer == null) {
            return;
        }
        final int desiredVisibility = shouldShowAndroidControls() ? View.VISIBLE : View.INVISIBLE;
        if (mControlContainer.getView().getVisibility() == desiredVisibility) return;
        mControlContainer.getView().removeCallbacks(mUpdateVisibilityRunnable);
        mControlContainer.getView().postOnAnimation(mUpdateVisibilityRunnable);
    }

    private void updateContentOffsetAndNotify() {
        TraceEvent.begin("FullscreenManager:updateContentOffsetAndNotify");

        updateContentViewChildrenState();

        int contentOffset = getContentOffset();
        if (mPreviousContentOffset != contentOffset) {
            for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                obs.onContentOffsetChanged(contentOffset);
            }
            mPreviousContentOffset = contentOffset;
        }

        TraceEvent.end("FullscreenManager:updateContentOffsetAndNotify");
    }

    /**
     * Forces the Android controls to hide. While there are acquired tokens the browser controls
     * Android view will always be hidden, otherwise they will show/hide based on position.
     *
     * NB: this only affects the Android controls. For controlling composited toolbar visibility,
     * implement {@link BrowserControlsVisibilityDelegate#canShowBrowserControls()}.
     */
    private int hideAndroidControls() {
        return mHidingTokenHolder.acquireToken();
    }

    /** Similar to {@link #hideAndroidControls}, but also replaces an old token */
    public int hideAndroidControlsAndClearOldToken(int oldToken) {
        int newToken = hideAndroidControls();
        mHidingTokenHolder.releaseToken(oldToken);
        return newToken;
    }

    /** Release a hiding token. */
    public void releaseAndroidControlsHidingToken(int token) {
        mHidingTokenHolder.releaseToken(token);
    }

    private boolean shouldShowAndroidControls() {
        if (mControlContainer == null) return false;
        if (mHidingTokenHolder.hasTokens()) {
            return false;
        }

        Tab tab = getTab();
        if (tab != null) {
            if (tab.isInitialized()) {
                if (offsetOverridden()) return true;
            } else {
                assert false : "Accessing a destroyed tab, setTab should have been called";
            }
        }

        boolean showControls = !drawControlsAsTexture();
        ViewGroup contentView = getContentView();
        if (contentView == null) return showControls;

        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof FrameLayout.LayoutParams)) continue;

            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) child.getLayoutParams();
            if (Gravity.TOP == (layoutParams.gravity & Gravity.FILL_VERTICAL)) {
                showControls = true;
                break;
            }
        }

        return showControls;
    }

    private void applyMarginToFullChildViews(
            ViewGroup contentView, float topMargin, float bottomMargin) {
        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof FrameLayout.LayoutParams)) continue;
            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) child.getLayoutParams();

            if (layoutParams.height == LayoutParams.MATCH_PARENT
                    && (layoutParams.topMargin != (int) topMargin
                               || layoutParams.bottomMargin != (int) bottomMargin)) {
                layoutParams.topMargin = (int) topMargin;
                layoutParams.bottomMargin = (int) bottomMargin;
                child.requestLayout();
                TraceEvent.instant("FullscreenManager:child.requestLayout()");
            }
        }
    }

    private void applyTranslationToTopChildViews(ViewGroup contentView, float translation) {
        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof FrameLayout.LayoutParams)) continue;

            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) child.getLayoutParams();
            if (Gravity.TOP == (layoutParams.gravity & Gravity.FILL_VERTICAL)) {
                child.setTranslationY(translation);
                TraceEvent.instant("FullscreenManager:child.setTranslationY()");
            }
        }
    }

    private ContentView getContentView() {
        Tab tab = getTab();
        return tab != null ? (ContentView) tab.getContentView() : null;
    }

    /**
     * Updates the positions of the browser controls and content to the default non fullscreen
     * values.
     */
    private void setPositionsForTabToNonFullscreen() {
        Tab tab = getTab();
        if (tab == null
                || TabBrowserControlsConstraintsHelper.getConstraints(tab)
                        != BrowserControlsState.HIDDEN) {
            setPositionsForTab(0, 0, getTopControlsHeight(), getTopControlsMinHeight(),
                    getBottomControlsMinHeight());
        } else {
            // Tab isn't null and the BrowserControlsState is HIDDEN. In this case, set the offsets
            // to values that will position the browser controls at the min-height.
            setPositionsForTab(getTopControlsMinHeight() - getTopControlsHeight(),
                    getBottomControlsHeight() - getBottomControlsMinHeight(),
                    getTopControlsMinHeight(), getTopControlsMinHeight(),
                    getBottomControlsMinHeight());
        }
    }

    /**
     * Updates the positions of the browser controls and content based on the desired position of
     * the current tab.
     * @param topControlsOffset The Y offset of the top controls in px.
     * @param bottomControlsOffset The Y offset of the bottom controls in px.
     * @param topContentOffset The Y offset for the content in px.
     * @param topControlsMinHeightOffset The Y offset for the top controls min-height in px.
     * @param bottomControlsMinHeightOffset The Y offset for the bottom controls min-height in px.
     */
    private void setPositionsForTab(int topControlsOffset, int bottomControlsOffset,
            int topContentOffset, int topControlsMinHeightOffset,
            int bottomControlsMinHeightOffset) {
        // This min/max logic is here to handle changes in the browser controls height. For example,
        // if we change either height to 0, the offsets of the controls should also be 0. This works
        // assuming we get an event from the renderer after the browser control heights change.
        int rendererTopControlOffset = Math.max(topControlsOffset, -getTopControlsHeight());
        int rendererBottomControlOffset = Math.min(bottomControlsOffset, getBottomControlsHeight());

        int rendererTopContentOffset =
                Math.min(topContentOffset, rendererTopControlOffset + getTopControlsHeight());

        if (rendererTopControlOffset == mRendererTopControlOffset
                && rendererBottomControlOffset == mRendererBottomControlOffset
                && rendererTopContentOffset == mRendererTopContentOffset
                && topControlsMinHeightOffset == mRendererTopControlsMinHeightOffset
                && bottomControlsMinHeightOffset == mRendererBottomControlsMinHeightOffset) {
            return;
        }

        mRendererTopControlOffset = rendererTopControlOffset;
        mRendererBottomControlOffset = rendererBottomControlOffset;
        mRendererTopControlsMinHeightOffset = topControlsMinHeightOffset;
        mRendererBottomControlsMinHeightOffset = bottomControlsMinHeightOffset;
        mRendererTopContentOffset = rendererTopContentOffset;

        mControlsAtMinHeight.set(getContentOffset() == getTopControlsMinHeight()
                && getBottomContentOffset() == getBottomControlsMinHeight());
        updateControlOffset();
        notifyControlOffsetChanged();
        updateContentOffsetAndNotify();
    }

    private void notifyControlOffsetChanged() {
        scheduleVisibilityUpdate();
        if (shouldShowAndroidControls()) {
            mControlContainer.getView().setTranslationY(getTopControlOffset());
        }

        // Whether we need the compositor to draw again to update our animation.
        // Should be |false| when the browser controls are only moved through the page
        // scrolling.
        boolean needsAnimate = shouldShowAndroidControls();
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onControlsOffsetChanged(getTopControlOffset(), getTopControlsMinHeightOffset(),
                    getBottomControlOffset(), getBottomControlsMinHeightOffset(), needsAnimate);
        }
    }

    /**
     * Notifies the fullscreen manager that a motion event has occurred.
     * @param e The dispatched motion event.
     */
    public void onMotionEvent(MotionEvent e) {
        int eventAction = e.getActionMasked();
        if (eventAction == MotionEvent.ACTION_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_DOWN) {
            mInGesture = true;
            // TODO(qinmin): Probably there is no need to hide the toast as it will go away
            // by itself.
            mHtmlApiHandler.hideNotificationToast();
        } else if (eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_UP) {
            mInGesture = false;
            updateContentOffsetAndNotify();
        }
    }

    /**
     * Called when offset values related with fullscreen functionality has been changed by the
     * compositor.
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param contentOffsetY The Y offset of the content in physical pixels.
     * @param topControlsMinHeightOffsetY The current offset of the top controls min-height.
     * @param bottomControlsMinHeightOffsetY The current offset of the bottom controls min-height.
     */
    private void onOffsetsChanged(int topControlsOffsetY, int bottomControlsOffsetY,
            int contentOffsetY, int topControlsMinHeightOffsetY,
            int bottomControlsMinHeightOffsetY) {
        // Cancel any animation on the Android controls and let compositor drive the offset updates.
        resetControlsOffsetOverridden();

        Tab tab = getTab();
        if (SadTab.isShowing(tab) || tab.isNativePage()) {
            showAndroidControls(false);
        } else {
            updateFullscreenManagerOffsets(false, topControlsOffsetY, bottomControlsOffsetY,
                    contentOffsetY, topControlsMinHeightOffsetY, bottomControlsMinHeightOffsetY);
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
            updateFullscreenManagerOffsets(true, 0, 0, getTopControlsHeight(),
                    getTopControlsMinHeight(), getBottomControlsMinHeight());
        }
    }

    /**
     * Restores the controls positions to the cached positions of the active Tab.
     */
    private void restoreControlsPositions() {
        resetControlsOffsetOverridden();

        // Make sure the dominant control offsets have been set.
        Tab tab = getTab();
        TabBrowserControlsOffsetHelper offsetHelper = null;
        if (tab != null) offsetHelper = TabBrowserControlsOffsetHelper.get(tab);

        if (offsetHelper != null && offsetHelper.offsetInitialized()) {
            updateFullscreenManagerOffsets(false, offsetHelper.topControlsOffset(),
                    offsetHelper.bottomControlsOffset(), offsetHelper.contentOffset(),
                    offsetHelper.topControlsMinHeightOffset(),
                    offsetHelper.bottomControlsMinHeightOffset());
        } else {
            showAndroidControls(false);
        }
        TabBrowserControlsConstraintsHelper.updateEnabledState(tab);
    }

    /**
     * Helper method to update offsets and notify offset changes to observers if necessary.
     */
    private void updateFullscreenManagerOffsets(boolean toNonFullscreen, int topControlsOffset,
            int bottomControlsOffset, int topContentOffset, int topControlsMinHeightOffset,
            int bottomControlsMinHeightOffset) {
        if (isInVr()) {
            rawTopContentOffsetChangedForVr();
            // The dip scale of java UI and WebContents are different while in VR, leading to a
            // mismatch in size in pixels when converting from dips. Since we hide the controls in
            // VR anyways, just set the offsets to what they're supposed to be with the controls
            // hidden.
            // TODO(mthiesse): Should we instead just set the top controls height to be 0 while in
            // VR?
            topControlsOffset = -getTopControlsHeight();
            bottomControlsOffset = getBottomControlsHeight();
            topContentOffset = 0;
            topControlsMinHeightOffset = 0;
            bottomControlsMinHeightOffset = 0;
            setPositionsForTab(topControlsOffset, bottomControlsOffset, topContentOffset,
                    topControlsMinHeightOffset, bottomControlsMinHeightOffset);
        } else if (toNonFullscreen) {
            setPositionsForTabToNonFullscreen();
        } else {
            setPositionsForTab(topControlsOffset, bottomControlsOffset, topContentOffset,
                    topControlsMinHeightOffset, bottomControlsMinHeightOffset);
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

        setOffsetOverridden(true);

        final float hiddenRatio = getBrowserControlHiddenRatio();
        final int topControlHeight = getTopControlsHeight();
        final int topControlOffset = getTopControlOffset();

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
                updateFullscreenManagerOffsets(false, 0, 0, topControlHeight,
                        getTopControlsMinHeight(), getBottomControlsMinHeight());
            }
        });
        mControlsAnimator.addUpdateListener((animator) -> {
            updateFullscreenManagerOffsets(false, (int) animator.getAnimatedValue(), 0,
                    topControlHeight, getTopControlsMinHeight(), getBottomControlsMinHeight());
        });
        mControlsAnimator.start();
    }

    // VR-related methods to make this class test-friendly. These are overridden in unit tests.

    protected boolean isInVr() {
        return VrModuleProvider.getDelegate().isInVr();
    }

    protected boolean bootsToVr() {
        return VrModuleProvider.getDelegate().bootsToVr();
    }

    protected void rawTopContentOffsetChangedForVr() {
        // TODO(https://crbug.com/1055619): VR wants to wait until the controls are fully hidden, as
        // otherwise there may be a brief race where the omnibox is rendered over the webcontents.
        // However, something seems to be happening in the case where the browser is launched on the
        // NTP, such that the top content offset is never set to 0. If we can figure out what that
        // is, we should be passing the TopContentOffset into this method again.
        VrModuleProvider.getDelegate().rawTopContentOffsetChanged(0);
    }

    @Override
    public void onEnterVr() {
        restoreControlsPositions();
    }

    @Override
    public void onExitVr() {
        // Clear the VR-specific overrides for controls height.
        restoreControlsPositions();

        // Show the Controls explicitly because under some situations, like when we're showing a
        // Native Page, the renderer won't send any new offsets.
        showAndroidControls(false);
    }

    /**
     * Destroys the FullscreenManager
     */
    public void destroy() {
        mTab = null;
        if (mActiveTabObserver != null) mActiveTabObserver.destroy();
        mBrowserVisibilityDelegate.destroy();
        if (mTabFullscreenObserver != null) mTabFullscreenObserver.destroy();
        if (mContentView != null) {
            mContentView.removeOnHierarchyChangeListener(this);
            mContentView.removeOnSystemUiVisibilityChangeListener(this);
        }
        VrModuleProvider.unregisterVrModeObserver(this);
    }

    @VisibleForTesting
    TabModelSelectorTabObserver getTabFullscreenObserverForTesting() {
        return mTabFullscreenObserver;
    }
}

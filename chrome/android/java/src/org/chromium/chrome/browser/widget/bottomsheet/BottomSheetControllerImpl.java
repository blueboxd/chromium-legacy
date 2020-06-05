// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.view.View;
import android.view.Window;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.vr.VrModeObserver;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.PriorityQueue;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
public class BottomSheetControllerImpl implements BottomSheetControllerInternal {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** A {@link VrModeObserver} that observers events of entering and exiting VR mode. */
    private final VrModeObserver mVrModeObserver;

    /** A listener for browser controls offset changes. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    /** A listener for fullscreen events. */
    private final ChromeFullscreenManager.FullscreenListener mFullscreenListener;

    /** A means of accessing the focus state of the omibox. */
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;

    /** An observer of the omnibox that suppresses the sheet when the omnibox is focused. */
    private final Callback<Boolean> mOmniboxFocusObserver;

    /** The height of the shadow that sits above the toolbar. */
    private int mToolbarShadowHeight;

    /** The offset of the toolbar shadow from the top that remains empty. */
    private int mShadowTopOffset;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private BottomSheet mBottomSheet;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** Whether the bottom sheet is temporarily suppressed. */
    private boolean mIsSuppressed;

    /** The manager for overlay panels to attach listeners to. */
    private Supplier<OverlayPanelManager> mOverlayPanelManager;

    /** A means for getting the activity's current tab and observing change events. */
    private ActivityTabProvider mTabProvider;

    /** A browser controls manager for polling browser controls offsets. */
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** A fullscreen manager for listening to fullscreen events. */
    private ChromeFullscreenManager mFullscreenManager;

    /** The last known activity tab, if available. */
    private Tab mLastActivityTab;

    /** A runnable that initializes the bottom sheet when necessary. */
    private Runnable mSheetInitializer;

    /**
     * A list of observers maintained by this controller until the bottom sheet is created, at which
     * point they will be added to the bottom  sheet.
     */
    private List<BottomSheetObserver> mPendingSheetObservers;

    /** The state of the sheet so it can be returned to what it was prior to suppression. */
    @SheetState
    private int mSheetStateBeforeSuppress;

    /** The content being shown prior to the sheet being suppressed. */
    private BottomSheetContent mContentWhenSuppressed;

    /** A means of accessing the ScrimCoordinator. */
    private Supplier<ScrimCoordinator> mScrimCoordinatorSupplier;

    /**
     * Build a new controller of the bottom sheet.
     * @param activityTabProvider The provider of the activity's current tab.
     * @param scrim A supplier of the scrim that shows when the bottom sheet is opened.
     * @param bottomSheetViewSupplier A mechanism for creating a {@link BottomSheet}.
     * @param overlayManager A supplier of the manager for overlay panels to attach listeners to.
     *                       This is a supplier to get around waiting for native to be initialized.
     * @param fullscreenManager A fullscreen manager for access to browser controls offsets.
     * @param omniboxFocusStateSupplier A means of accessing the focused state of the omnibox.
     */
    public BottomSheetControllerImpl(final ActivityTabProvider activityTabProvider,
            final Supplier<ScrimCoordinator> scrim, Supplier<View> bottomSheetViewSupplier,
            Supplier<OverlayPanelManager> overlayManager, ChromeFullscreenManager fullscreenManager,
            Window window, KeyboardVisibilityDelegate keyboardDelegate,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mTabProvider = activityTabProvider;
        mOverlayPanelManager = overlayManager;
        mBrowserControlsStateProvider = fullscreenManager;
        mFullscreenManager = fullscreenManager;
        mScrimCoordinatorSupplier = scrim;
        mPendingSheetObservers = new ArrayList<>();

        mPendingSheetObservers.add(new EmptyBottomSheetObserver() {
            /** The token used to enable browser controls persistence. */
            private int mPersistentControlsToken;

            @Override
            public void onSheetOpened(int reason) {
                if (mFullscreenManager.getBrowserVisibilityDelegate() == null) return;

                // Browser controls should stay visible until the sheet is closed.
                mPersistentControlsToken =
                        mFullscreenManager.getBrowserVisibilityDelegate().showControlsPersistent();
            }

            @Override
            public void onSheetClosed(int reason) {
                if (mFullscreenManager.getBrowserVisibilityDelegate() == null) return;

                // Update the browser controls since they are permanently shown while the sheet is
                // open.
                mFullscreenManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                        mPersistentControlsToken);
            }
        });

        mVrModeObserver = new VrModeObserver() {
            @Override
            public void onEnterVr() {
                suppressSheet(StateChangeReason.VR);
            }

            @Override
            public void onExitVr() {
                unsuppressSheet();
            }
        };

        mBrowserControlsObserver = new Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                if (mBottomSheet != null) {
                    mBottomSheet.setBrowserControlsHiddenRatio(
                            mBrowserControlsStateProvider.getBrowserControlHiddenRatio());
                }
            }
        };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);

        mFullscreenListener = new ChromeFullscreenManager.FullscreenListener() {
            @Override
            public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                if (mBottomSheet == null || mTabProvider.get() != tab) return;
                suppressSheet(StateChangeReason.COMPOSITED_UI);
            }

            @Override
            public void onExitFullscreen(Tab tab) {
                if (mBottomSheet == null || mTabProvider.get() != tab) return;
                unsuppressSheet();
            }
        };
        mFullscreenManager.addListener(mFullscreenListener);

        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mOmniboxFocusObserver = (focused) -> {
            if (focused) {
                suppressSheet(StateChangeReason.NONE);
            } else {
                unsuppressSheet();
            }
        };

        mSheetInitializer = () -> {
            initializeSheet(bottomSheetViewSupplier, window, keyboardDelegate);
        };
    }

    /**
     * Do the actual initialization of the bottom sheet.
     * @param bottomSheetViewSupplier A means of creating the bottom sheet.
     * @param window A means of accessing the screen size.
     * @param keyboardDelegate A means of hiding the keyboard.
     */
    private void initializeSheet(Supplier<View> bottomSheetViewSupplier, Window window,
            KeyboardVisibilityDelegate keyboardDelegate) {
        mBottomSheet = (BottomSheet) bottomSheetViewSupplier.get();
        mBottomSheet.init(window, keyboardDelegate);
        mToolbarShadowHeight = mBottomSheet.getResources().getDimensionPixelOffset(
                BottomSheet.getTopShadowResourceId());
        mShadowTopOffset = mBottomSheet.getResources().getDimensionPixelOffset(
                BottomSheet.getShadowTopOffsetResourceId());
        mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content1.getPriority() - content2.getPriority());

        VrModuleProvider.registerVrModeObserver(mVrModeObserver);

        final TabObserver tabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                clearRequestsAndHide();
            }

            @Override
            public void onCrash(Tab tab) {
                clearRequestsAndHide();
            }

            @Override
            public void onDestroyed(Tab tab) {
                if (mLastActivityTab != tab) return;
                mLastActivityTab = null;

                // Remove the suppressed sheet if its lifecycle is tied to the tab being
                // destroyed.
                clearRequestsAndHide();
            }
        };

        mTabProvider.addObserverAndTrigger(new HintlessActivityTabObserver() {
            @Override
            public void onActivityTabChanged(Tab tab) {
                // Temporarily suppress the sheet if entering a state where there is no activity
                // tab.
                if (tab == null) {
                    suppressSheet(StateChangeReason.COMPOSITED_UI);
                    return;
                }

                // If refocusing the same tab, simply unsuppress the sheet.
                if (mLastActivityTab == tab) {
                    unsuppressSheet();
                    return;
                }

                // Move the observer to the new activity tab and clear the sheet.
                if (mLastActivityTab != null) mLastActivityTab.removeObserver(tabObserver);
                mLastActivityTab = tab;
                mLastActivityTab.addObserver(tabObserver);
                clearRequestsAndHide();
            }
        });

        PropertyModel scrimProperties =
                new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.ANCHOR_VIEW, mBottomSheet)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.CLICK_DELEGATE,
                                () -> {
                                    if (!mBottomSheet.isSheetOpen()) return;
                                    mBottomSheet.setSheetState(
                                            mBottomSheet.getMinSwipableSheetState(), true,
                                            StateChangeReason.TAP_SCRIM);
                                })
                        .build();

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            /**
             * Whether the scrim was shown for the last content.
             * TODO(mdjones): We should try to make sure the content in the sheet is not nulled
             *                prior to the close event occurring; sheets that don't have a peek
             *                state make this difficult since the sheet needs to be hidden before it
             *                is closed.
             */
            private boolean mScrimShown;

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (mBottomSheet.getCurrentSheetContent() != null
                        && mBottomSheet.getCurrentSheetContent().hasCustomScrimLifecycle()) {
                    return;
                }

                mScrimCoordinatorSupplier.get().showScrim(scrimProperties);
                mScrimShown = true;
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                // Hide the scrim if the current content doesn't have a custom scrim lifecycle.
                if (mScrimShown) {
                    mScrimCoordinatorSupplier.get().hideScrim(true);
                    mScrimShown = false;
                }

                // Try to swap contents unless the sheet's content has a custom lifecycle.
                if (mBottomSheet.getCurrentSheetContent() != null
                        && !mBottomSheet.getCurrentSheetContent().hasCustomLifecycle()) {
                    // If the sheet is closed, it is an opportunity for another content to try to
                    // take its place if it is a higher priority.
                    BottomSheetContent content = mBottomSheet.getCurrentSheetContent();
                    BottomSheetContent nextContent = mContentQueue.peek();
                    if (content != null && nextContent != null
                            && nextContent.getPriority() < content.getPriority()) {
                        mContentQueue.add(content);
                        mBottomSheet.setSheetState(SheetState.HIDDEN, true);
                    }
                }
            }

            @Override
            public void onSheetStateChanged(@SheetState int state) {
                // If hiding request is in progress, destroy the current sheet content being hidden
                // even when it is in suppressed state. See https://crbug.com/1057966.
                if (state != SheetState.HIDDEN || (!mIsProcessingHideRequest && mIsSuppressed)) {
                    return;
                }
                if (mBottomSheet.getCurrentSheetContent() != null) {
                    mBottomSheet.getCurrentSheetContent().destroy();
                }
                mIsProcessingHideRequest = false;
                showNextContent(true);
            }
        });

        // Add any of the pending observers that were added prior to the sheet being created.
        for (int i = 0; i < mPendingSheetObservers.size(); i++) {
            mBottomSheet.addObserver(mPendingSheetObservers.get(i));
        }
        mPendingSheetObservers.clear();

        mSheetInitializer = null;
    }

    @Override
    public ScrimCoordinator getScrimCoordinator() {
        return mScrimCoordinatorSupplier.get();
    }

    @Override
    public PropertyModel createScrimParams() {
        return new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                .with(ScrimProperties.TOP_MARGIN, 0)
                .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                .with(ScrimProperties.ANCHOR_VIEW, mBottomSheet)
                .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                .with(ScrimProperties.CLICK_DELEGATE,
                        () -> {
                            if (!mBottomSheet.isSheetOpen()) return;
                            mBottomSheet.setSheetState(mBottomSheet.getMinSwipableSheetState(),
                                    true, StateChangeReason.TAP_SCRIM);
                        })
                .build();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);
        mFullscreenManager.removeListener(mFullscreenListener);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        if (mBottomSheet != null) mBottomSheet.destroy();
    }

    @Override
    public boolean handleBackPress() {
        // If suppressed (therefore invisible), users are likely to expect for Chrome
        // browser, not the bottom sheet, to react. Do not consume the event.
        if (mBottomSheet == null || mIsSuppressed) return false;

        // Give the sheet the opportunity to handle the back press itself before falling to the
        // default "close" behavior.
        if (getCurrentSheetContent() != null && getCurrentSheetContent().handleBackPress()) {
            return true;
        }

        if (!mBottomSheet.isSheetOpen()) return false;
        int sheetState = mBottomSheet.getMinSwipableSheetState();
        mBottomSheet.setSheetState(sheetState, true, StateChangeReason.BACK_PRESS);
        return true;
    }

    @Override
    public BottomSheetContent getCurrentSheetContent() {
        return mBottomSheet == null ? null : mBottomSheet.getCurrentSheetContent();
    }

    @Override
    @SheetState
    public int getSheetState() {
        return mBottomSheet == null ? SheetState.HIDDEN : mBottomSheet.getSheetState();
    }

    @Override
    @SheetState
    public int getTargetSheetState() {
        return mBottomSheet == null ? SheetState.NONE : mBottomSheet.getTargetSheetState();
    }

    @Override
    public boolean isSheetOpen() {
        return mBottomSheet != null && mBottomSheet.isSheetOpen();
    }

    @Override
    public boolean isSheetHiding() {
        return mBottomSheet == null ? false : mBottomSheet.isHiding();
    }

    @Override
    public int getCurrentOffset() {
        return mBottomSheet == null ? 0 : (int) mBottomSheet.getCurrentOffsetPx();
    }

    @Override
    public int getContainerHeight() {
        return mBottomSheet != null ? (int) mBottomSheet.getSheetContainerHeight() : 0;
    }

    @Override
    public int getTopShadowHeight() {
        return mToolbarShadowHeight + mShadowTopOffset;
    }

    @Override
    public void addObserver(BottomSheetObserver observer) {
        if (mBottomSheet == null) {
            mPendingSheetObservers.add(observer);
            return;
        }
        mBottomSheet.addObserver(observer);
    }

    @Override
    public void removeObserver(BottomSheetObserver observer) {
        if (mBottomSheet != null) {
            mBottomSheet.removeObserver(observer);
        } else {
            mPendingSheetObservers.remove(observer);
        }
    }

    @Override
    public void suppressSheet(@StateChangeReason int reason) {
        mSheetStateBeforeSuppress = getSheetState();
        mContentWhenSuppressed = getCurrentSheetContent();
        mIsSuppressed = true;
        mBottomSheet.setSheetState(SheetState.HIDDEN, false, reason);
    }

    @Override
    public void unsuppressSheet() {
        if (!mIsSuppressed || mTabProvider.get() == null || VrModuleProvider.getDelegate().isInVr()
                || mOmniboxFocusStateSupplier.get()) {
            return;
        }
        mIsSuppressed = false;

        if (mBottomSheet.getCurrentSheetContent() != null) {
            @SheetState
            int openState = mContentWhenSuppressed == getCurrentSheetContent()
                    ? mSheetStateBeforeSuppress
                    : mBottomSheet.getOpeningState();
            mBottomSheet.setSheetState(openState, true);
        } else {
            // In the event the previous content was hidden, try to show the next one.
            showNextContent(true);
        }
        mContentWhenSuppressed = null;
        mSheetStateBeforeSuppress = SheetState.NONE;
    }

    @VisibleForTesting
    void setSheetStateForTesting(@SheetState int state, boolean animate) {
        mBottomSheet.setSheetState(state, animate);
    }

    @VisibleForTesting
    View getBottomSheetViewForTesting() {
        return mBottomSheet;
    }

    /**
     * WARNING: This destroys the internal sheet state. Only use in tests and only use once!
     *
     * To simulate scrolling, this method puts the sheet in a permanent scrolling state.
     * @return The target state of the bottom sheet (to check thresholds).
     */
    @VisibleForTesting
    @SheetState
    int forceScrollingForTesting(float sheetHeight, float yVelocity) {
        return mBottomSheet.forceScrollingStateForTesting(sheetHeight, yVelocity);
    }

    @VisibleForTesting
    public void endAnimationsForTesting() {
        mBottomSheet.endAnimations();
    }

    @Override
    public boolean requestShowContent(BottomSheetContent content, boolean animate) {
        if (mBottomSheet == null) mSheetInitializer.run();

        // If already showing the requested content, do nothing.
        if (content == mBottomSheet.getCurrentSheetContent()) return true;

        // Showing the sheet requires a tab.
        if (mTabProvider.get() == null) return false;

        boolean shouldSuppressExistingContent = mBottomSheet.getCurrentSheetContent() != null
                && content.getPriority() < mBottomSheet.getCurrentSheetContent().getPriority()
                && canBottomSheetSwitchContent();

        // Always add the content to the queue, it will be handled after the sheet closes if
        // necessary. If already hidden, |showNextContent| will handle the request.
        mContentQueue.add(content);

        if (mBottomSheet.getCurrentSheetContent() == null) {
            showNextContent(animate);
            return true;
        } else if (shouldSuppressExistingContent) {
            mContentQueue.add(mBottomSheet.getCurrentSheetContent());
            mBottomSheet.setSheetState(SheetState.HIDDEN, animate);
            return true;
        }
        return false;
    }

    @Override
    public void hideContent(
            BottomSheetContent content, boolean animate, @StateChangeReason int hideReason) {
        if (mBottomSheet == null) return;

        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == SheetState.HIDDEN) {
            // If the sheet is already hidden, simply show the next content.
            showNextContent(animate);
        } else {
            mIsProcessingHideRequest = true;
            mBottomSheet.setSheetState(SheetState.HIDDEN, animate, hideReason);
        }
    }

    @Override
    public void hideContent(BottomSheetContent content, boolean animate) {
        hideContent(content, animate, StateChangeReason.NONE);
    }

    @Override
    public void expandSheet() {
        if (mBottomSheet == null || mIsSuppressed) return;

        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(SheetState.HALF, true);
        if (mOverlayPanelManager.get().getActivePanel() != null) {
            // TODO(mdjones): This should only apply to contextual search, but contextual search is
            //                the only implementation. Fix this to only apply to contextual search.
            mOverlayPanelManager.get().getActivePanel().closePanel(
                    OverlayPanel.StateChangeReason.UNKNOWN, true);
        }
    }

    @Override
    public boolean collapseSheet(boolean animate) {
        if (mBottomSheet == null || mIsSuppressed) return false;
        if (mBottomSheet.isSheetOpen() && mBottomSheet.isPeekStateEnabled()) {
            mBottomSheet.setSheetState(SheetState.PEEK, animate);
            return true;
        }
        return false;
    }

    /**
     * Show the next {@link BottomSheetContent} if it is available and peek the sheet. If no content
     * is available the sheet's content is set to null.
     * @param animate Whether the sheet should animate opened.
     */
    private void showNextContent(boolean animate) {
        if (mContentQueue.isEmpty()) {
            mBottomSheet.showContent(null);
            return;
        }

        BottomSheetContent nextContent = mContentQueue.poll();
        mBottomSheet.showContent(nextContent);
        mBottomSheet.setSheetState(mBottomSheet.getOpeningState(), animate);
    }

    @Override
    public void clearRequestsAndHide() {
        clearRequests(mContentQueue.iterator());

        BottomSheetContent currentContent = mBottomSheet.getCurrentSheetContent();
        if (currentContent == null || !currentContent.hasCustomLifecycle()) {
            if (mContentQueue.isEmpty()) mIsSuppressed = false;

            hideContent(currentContent, /* animate= */ true);
        }
        mContentWhenSuppressed = null;
        mSheetStateBeforeSuppress = SheetState.NONE;
    }

    /**
     * Remove all contents from {@code iterator} that don't have a custom lifecycle.
     * @param iterator The iterator whose items must be removed.
     */
    private void clearRequests(Iterator<BottomSheetContent> iterator) {
        while (iterator.hasNext()) {
            if (!iterator.next().hasCustomLifecycle()) {
                iterator.remove();
            }
        }
    }

    /**
     * The bottom sheet cannot change content while it is open. If the user has the bottom sheet
     * open, they are currently engaged in a task and shouldn't be interrupted.
     * @return Whether the sheet currently supports switching its content.
     */
    private boolean canBottomSheetSwitchContent() {
        return !mBottomSheet.isSheetOpen();
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import androidx.annotation.IntDef;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
public class BottomSheetController implements Destroyable {
    /** The different states that the bottom sheet can have. */
    @IntDef({SheetState.NONE, SheetState.HIDDEN, SheetState.PEEK, SheetState.HALF, SheetState.FULL,
            SheetState.SCROLLING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetState {
        /**
         * NONE is for internal use only and indicates the sheet is not currently
         * transitioning between states.
         */
        int NONE = -1;
        // Values are used for indexing mStateRatios, should start from 0
        // and can't have gaps. Additionally order is important for these,
        // they go from smallest to largest.
        int HIDDEN = 0;
        int PEEK = 1;
        int HALF = 2;
        int FULL = 3;

        int SCROLLING = 4;
    }

    /**
     * The different reasons that the sheet's state can change.
     *
     * Needs to stay in sync with BottomSheet.StateChangeReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({StateChangeReason.NONE, StateChangeReason.SWIPE, StateChangeReason.BACK_PRESS,
            StateChangeReason.TAP_SCRIM, StateChangeReason.NAVIGATION,
            StateChangeReason.COMPOSITED_UI, StateChangeReason.VR, StateChangeReason.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateChangeReason {
        int NONE = 0;
        int SWIPE = 1;
        int BACK_PRESS = 2;
        int TAP_SCRIM = 3;
        int NAVIGATION = 4;
        int COMPOSITED_UI = 5;
        int VR = 6;
        int MAX_VALUE = VR;
    }

    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** A {@link VrModeObserver} that observers events of entering and exiting VR mode. */
    private final VrModeObserver mVrModeObserver;

    /** The parameters that control how the scrim behaves while the sheet is open. */
    private ScrimParams mScrimParams;

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

    /** The last known activity tab, if available. */
    private Tab mLastActivityTab;

    /** A runnable that initializes the bottom sheet when necessary. */
    private Runnable mSheetInitializer;

    /**
     * A list of observers maintained by this controller until the bottom sheet is created, at which
     * point they will be added to the bottom  sheet.
     */
    private List<BottomSheetObserver> mPendingSheetObservers;

    /**
     * Build a new controller of the bottom sheet.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the {@code activity}.
     * @param activityTabProvider The provider of the activity's current tab.
     * @param scrim The scrim that shows when the bottom sheet is opened.
     * @param bottomSheetSupplier A mechanism for creating a {@link BottomSheet}.
     * @param overlayManager A supplier of the manager for overlay panels to attach listeners to.
     *                       This is a supplier to get around wating for native to be initialized.
     */
    public BottomSheetController(final ActivityLifecycleDispatcher lifecycleDispatcher,
            final ActivityTabProvider activityTabProvider, final ScrimView scrim,
            Supplier<BottomSheet> bottomSheetSupplier,
            Supplier<OverlayPanelManager> overlayManager) {
        mTabProvider = activityTabProvider;
        mOverlayPanelManager = overlayManager;
        mPendingSheetObservers = new ArrayList<>();

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

        mSheetInitializer = () -> initializeSheet(lifecycleDispatcher, scrim, bottomSheetSupplier);
    }

    /**
     * Do the actual initialization of the bottom sheet.
     * @param lifecycleDispatcher A means of binding to the activity's lifecycle.
     * @param scrim The scrim to show behind the sheet.
     * @param bottomSheetSupplier A means of creating the bottom sheet.
     */
    private void initializeSheet(final ActivityLifecycleDispatcher lifecycleDispatcher,
            final ScrimView scrim, Supplier<BottomSheet> bottomSheetSupplier) {
        mBottomSheet = bottomSheetSupplier.get();

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content1.getPriority() - content2.getPriority());

        lifecycleDispatcher.register(this);

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

        ScrimObserver scrimObserver = new ScrimObserver() {
            @Override
            public void onScrimClick() {
                if (!mBottomSheet.isSheetOpen()) return;
                mBottomSheet.setSheetState(
                        mBottomSheet.getMinSwipableSheetState(), true, StateChangeReason.TAP_SCRIM);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mScrimParams = new ScrimParams(mBottomSheet, false, true, 0, scrimObserver);

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (mBottomSheet.getCurrentSheetContent() != null
                        && mBottomSheet.getCurrentSheetContent().hasCustomScrimLifecycle()) {
                    return;
                }

                scrim.showScrim(mScrimParams);
                scrim.setViewAlpha(0);
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                if (mBottomSheet.getCurrentSheetContent() != null
                        && mBottomSheet.getCurrentSheetContent().hasCustomScrimLifecycle()) {
                    return;
                }

                scrim.hideScrim(true);

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

            @Override
            public void onSheetStateChanged(@SheetState int state) {
                if (state != SheetState.HIDDEN || mIsSuppressed) return;
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

    // Destroyable implementation.
    @Override
    public void destroy() {
        VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);
        if (mBottomSheet != null) mBottomSheet.destroy();
    }

    /**
     * Handle a back press event. If the sheet is open it will be closed.
     * @return True if the sheet handled the back press.
     */
    public boolean handleBackPress() {
        if (mBottomSheet == null || !mBottomSheet.isSheetOpen()) return false;

        int sheetState = mBottomSheet.getMinSwipableSheetState();
        mBottomSheet.setSheetState(sheetState, true, StateChangeReason.BACK_PRESS);
        return true;
    }

    /** @return The content currently showing in the bottom sheet. */
    public BottomSheetContent getCurrentSheetContent() {
        return mBottomSheet == null ? null : mBottomSheet.getCurrentSheetContent();
    }

    /** @return The current state of the bottom sheet. */
    @SheetState
    public int getSheetState() {
        return mBottomSheet == null ? SheetState.HIDDEN : mBottomSheet.getSheetState();
    }

    /** @return The target state of the bottom sheet (usually during animations). */
    @SheetState
    public int getTargetSheetState() {
        return mBottomSheet == null ? SheetState.NONE : mBottomSheet.getTargetSheetState();
    }

    /** @return Whether the bottom sheet is currently open (expanded beyond peek state). */
    public boolean isSheetOpen() {
        return mBottomSheet != null && mBottomSheet.isSheetOpen();
    }

    /** @return Whether the bottom sheet is in the process of hiding. */
    public boolean isSheetHiding() {
        return mBottomSheet == null ? false : mBottomSheet.isHiding();
    }

    /**
     * @param observer The observer to add.
     */
    public void addObserver(BottomSheetObserver observer) {
        if (mBottomSheet == null) {
            mPendingSheetObservers.add(observer);
            return;
        }
        mBottomSheet.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(BottomSheetObserver observer) {
        if (mBottomSheet != null) {
            mBottomSheet.removeObserver(observer);
        } else {
            mPendingSheetObservers.remove(observer);
        }
    }

    /**
     * Temporarily suppress the bottom sheet while other UI is showing. This will not itself change
     * the content displayed by the sheet.
     * @param reason The reason the sheet was suppressed.
     */
    private void suppressSheet(@StateChangeReason int reason) {
        mIsSuppressed = true;
        mBottomSheet.setSheetState(SheetState.HIDDEN, false, reason);
    }

    /**
     * Unsuppress the bottom sheet. This may or may not affect the sheet depending on the state of
     * the browser (i.e. the tab switcher may be showing).
     */
    private void unsuppressSheet() {
        if (!mIsSuppressed || mTabProvider.get() == null || isOtherUIObscuring()
                || VrModuleProvider.getDelegate().isInVr()) {
            return;
        }
        mIsSuppressed = false;

        if (mBottomSheet.getCurrentSheetContent() != null) {
            mBottomSheet.setSheetState(mBottomSheet.getOpeningState(), true);
        } else {
            // In the event the previous content was hidden, try to show the next one.
            showNextContent(true);
        }
    }

    /**
     * @return The {@link BottomSheet} controlled by this class.
     */
    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    @VisibleForTesting
    public void setSheetStateForTesting(@SheetState int state, boolean animate) {
        mBottomSheet.setSheetState(state, animate);
    }

    /**
     * Request that some content be shown in the bottom sheet.
     * @param content The content to be shown in the bottom sheet.
     * @param animate Whether the appearance of the bottom sheet should be animated.
     * @return True if the content was shown, false if it was suppressed. Content is suppressed if
     *         higher priority content is in the sheet, the sheet is expanded beyond the peeking
     *         state, or the browser is in a mode that does not support showing the sheet.
     */
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

    /**
     * Hide content shown in the bottom sheet. If the content is not showing, this call retracts the
     * request to show it.
     * @param content The content to be hidden.
     * @param animate Whether the sheet should animate when hiding.
     */
    public void hideContent(BottomSheetContent content, boolean animate) {
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
            mBottomSheet.setSheetState(SheetState.HIDDEN, animate);
        }
    }

    /**
     * Expand the {@link BottomSheet}. If there is no content in the sheet, this is a noop.
     */
    public void expandSheet() {
        if (mBottomSheet == null) return;

        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(SheetState.HALF, true);
        if (mOverlayPanelManager.get().getActivePanel() != null) {
            // TODO(mdjones): This should only apply to contextual search, but contextual search is
            //                the only implementation. Fix this to only apply to contextual search.
            mOverlayPanelManager.get().getActivePanel().closePanel(
                    OverlayPanel.StateChangeReason.UNKNOWN, true);
        }
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

    /**
     * For all contents that don't have a custom lifecycle, we remove them from show requests or
     * hide it if it is currently shown.
     */
    private void clearRequestsAndHide() {
        clearRequests(mContentQueue.iterator());

        BottomSheetContent currentContent = mBottomSheet.getCurrentSheetContent();
        if (currentContent == null || !currentContent.hasCustomLifecycle()) {
            if (mContentQueue.isEmpty()) mIsSuppressed = false;

            hideContent(currentContent, /* animate= */ true);
        }
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
     * @return Whether some other UI is preventing the sheet from showing.
     */
    protected boolean isOtherUIObscuring() {
        return mOverlayPanelManager.get() != null
                && mOverlayPanelManager.get().getActivePanel() != null;
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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.app.Activity;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;

import java.util.Iterator;
import java.util.PriorityQueue;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
public class BottomSheetController implements Destroyable {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** The parameters that controll how the scrim behaves while the sheet is open. */
    private final ScrimParams mScrimParams;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private final BottomSheet mBottomSheet;

    /** A handle to the {@link SnackbarManager} that manages snackbars inside the bottom sheet. */
    private final SnackbarManager mSnackbarManager;

    /** A {@link VrModeObserver} that observers events of entering and exiting VR mode. */
    private final VrModeObserver mVrModeObserver;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** Whether the bottom sheet is temporarily suppressed. */
    private boolean mIsSuppressed;

    /** The manager for overlay panels to attach listeners to. */
    private OverlayPanelManager mOverlayPanelManager;

    /** A means for getting the activity's current tab and observing change events. */
    private ActivityTabProvider mTabProvider;

    /** The last known activity tab, if available. */
    private Tab mLastActivityTab;

    /**
     * Build a new controller of the bottom sheet.
     * @param activity An activity for context.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the {@code activity}.
     * @param activityTabProvider The provider of the activity's current tab.
     * @param scrim The scrim that shows when the bottom sheet is opened.
     * @param bottomSheet The bottom sheet that this class will be controlling.
     * @param overlayManager The manager for overlay panels to attach listeners to.
     */
    public BottomSheetController(final Activity activity,
            final ActivityLifecycleDispatcher lifecycleDispatcher,
            final ActivityTabProvider activityTabProvider, final ScrimView scrim,
            BottomSheet bottomSheet, OverlayPanelManager overlayManager) {
        mBottomSheet = bottomSheet;
        mTabProvider = activityTabProvider;
        mOverlayPanelManager = overlayManager;
        mSnackbarManager = new SnackbarManager(
                activity, mBottomSheet.findViewById(R.id.bottom_sheet_snackbar_container));

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content1.getPriority() - content2.getPriority());

        lifecycleDispatcher.register(this);

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
                if (mLastActivityTab == tab) mLastActivityTab = null;
            }
        };

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
        VrModuleProvider.registerVrModeObserver(mVrModeObserver);

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
                        mBottomSheet.getCurrentSheetContent().isPeekStateEnabled()
                                ? BottomSheet.SheetState.PEEK
                                : BottomSheet.SheetState.HIDDEN,
                        true, StateChangeReason.TAP_SCRIM);
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

                // If the sheet is closed, it is an opportunity for another content to try to take
                // its place if it is a higher priority.
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
                mIsProcessingHideRequest = false;
                showNextContent(true);
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                mSnackbarManager.dismissAllSnackbars();
            }
        });
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);
        mBottomSheet.destroy();
    }

    /**
     * Temporarily suppress the bottom sheet while other UI is showing. This will not itself change
     * the content displayed by the sheet.
     * @param reason The reason the sheet was suppressed.
     */
    private void suppressSheet(@StateChangeReason int reason) {
        mIsSuppressed = true;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, false, reason);
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
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, true);
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

    /**
     * @return The {@link SnackbarManager} that manages snackbars inside the bottom sheet.
     */
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
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
        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == BottomSheet.SheetState.HIDDEN) {
            // If the sheet is already hidden, simply show the next content.
            showNextContent(animate);
        } else {
            mIsProcessingHideRequest = true;
            mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, animate);
        }
    }

    /**
     * Expand the {@link BottomSheet}. If there is no content in the sheet, this is a noop.
     */
    public void expandSheet() {
        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HALF, true);
        if (mOverlayPanelManager.getActivePanel() != null) {
            // TODO(mdjones): This should only apply to contextual search, but contextual search is
            //                the only implementation. Fix this to only apply to contextual search.
            mOverlayPanelManager.getActivePanel().closePanel(
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
        mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, animate);
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
        return mOverlayPanelManager.getActivePanel() != null;
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

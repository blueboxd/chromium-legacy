// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.Supplier;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.gesturenav.NavigationSheetMediator.ItemProperties;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.ContentPriority;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator class for navigation sheet.
 * TODO(jinsukkim): Write tests.
 */
class NavigationSheetCoordinator implements BottomSheetContent, NavigationSheet {
    // Type of the navigation list item. We have only single type.
    static final int NAVIGATION_LIST_ITEM_TYPE_ID = 0;

    // Amount of time to hold the finger still to trigger navigation bottom sheet.
    // This ensures fling gestures from edge won't invoke the  sheet.
    private final static int PEEK_HOLD_DELAY_MS = 50;

    // Navigation sheet is triggered into peek state if swiped by this much or more from an edge.
    // Actual amount is capped so it is at most half the screen width.
    private static final int PEEK_THRESHOLD_DP = 224;

    private final NavigationSheetView mContentView;
    private final View mToolbarView;
    private final LayoutInflater mLayoutInflater;
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final NavigationSheet.Delegate mDelegate;
    private final NavigationSheetMediator mMediator;
    private final BottomSheetObserver mSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            close(false);
        }
    };

    private final Handler mHandler = new Handler();
    private final Runnable mSheetPeekRunnable;
    private final float mPeekSheetThreshold;

    private final ModelList mModelList = new ModelList();
    private final ModelListAdapter mModelAdapter = new ModelListAdapter(mModelList);

    private static class NavigationItemViewBinder {
        public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
            if (ItemProperties.ICON == propertyKey) {
                ((ImageView) view.findViewById(R.id.favicon_img))
                        .setImageDrawable(model.get(ItemProperties.ICON));
            } else if (ItemProperties.LABEL == propertyKey) {
                ((TextView) view.findViewById(R.id.entry_title))
                        .setText(model.get(ItemProperties.LABEL));
            } else if (ItemProperties.CLICK_LISTENER == propertyKey) {
                view.setOnClickListener(model.get(ItemProperties.CLICK_LISTENER));
            }
        }
    }

    // Whether the navigation sheet is visible. Deemed true if it was requested to show even though
    // it is not immediately visible. This is to avoid making repeated show/hide requests.
    private boolean mSheetVisible;

    private boolean mForward;

    private boolean mShowCloseIndicator;

    // Metrics. True if sheet has ever been triggered (in peeked state) for an edge swipe.
    private boolean mSheetTriggered;

    /**
     * Construct a new NavigationSheet.
     */
    NavigationSheetCoordinator(
            View parent, Supplier<BottomSheetController> bottomSheetController, Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;
        Context context = parent.getContext();
        mLayoutInflater = LayoutInflater.from(context);
        mToolbarView = mLayoutInflater.inflate(R.layout.navigation_sheet_toolbar, null);
        mContentView =
                (NavigationSheetView) mLayoutInflater.inflate(R.layout.navigation_sheet, null);
        mMediator = new NavigationSheetMediator(context, mModelList, (position, index) -> {
            mDelegate.navigateToIndex(index);
            close(false);
            GestureNavMetrics.recordHistogram("GestureNavigation.Sheet.Used", mForward);

            // Logs position of the clicked item. Back navigation has negative value,
            // while forward positive. Show full history is zero.
            RecordHistogram.recordSparseHistogram("GestureNavigation.Sheet.Selected",
                    index == -1 ? 0 : (mForward ? position + 1 : -position - 1));
        });
        mModelAdapter.registerType(NAVIGATION_LIST_ITEM_TYPE_ID, () -> {
            return mLayoutInflater.inflate(R.layout.navigation_popup_item, null);
        }, NavigationItemViewBinder::bind);
        ListView listview = (ListView) mContentView.findViewById(R.id.navigation_entries);
        listview.setAdapter(mModelAdapter);

        mSheetPeekRunnable = () -> {
            if (!isVisible()) peek(mForward);
        };

        mPeekSheetThreshold =
                Math.min(context.getResources().getDisplayMetrics().density * PEEK_THRESHOLD_DP,
                        parent.getWidth() / 2);
    }

    // NavigationSheet

    @Override
    public void start(boolean forward, boolean showCloseIndicator) {
        if (mBottomSheetController.get() == null) return;
        mForward = forward;
        mShowCloseIndicator = showCloseIndicator;
        setVisible(false);
        mSheetTriggered = false;
    }

    @Override
    public void onScroll(float delta, float overscroll) {
        if (mBottomSheetController.get() == null) return;
        if (mShowCloseIndicator) return;
        if (overscroll > mPeekSheetThreshold) {
            if (isHidden() && Math.abs(delta) > 2.f) {
                mHandler.removeCallbacks(mSheetPeekRunnable);
                mHandler.postDelayed(mSheetPeekRunnable, PEEK_HOLD_DELAY_MS);
            }
        } else if (isPeeked()) {
            close(true);
        }
    }

    @Override
    public void release() {
        if (mBottomSheetController.get() == null) return;
        mHandler.removeCallbacks(mSheetPeekRunnable);
        // Show navigation sheet if released at peek state.
        if (mSheetTriggered) {
            GestureNavMetrics.recordHistogram("GestureNavigation.Sheet.Peeked", mForward);
        }
        if (isPeeked()) {
            mBottomSheetController.get().expandSheet();
            GestureNavMetrics.recordHistogram("GestureNavigation.Sheet.Viewed", mForward);
        }
    }

    /**
     * Move the navigation sheet to peek state.
     * @param forward {@code true} if the gesture is for navigating forward.
     */
    private void peek(boolean forward) {
        mMediator.populateEntries(mDelegate.getHistory(forward));
        mBottomSheetController.get().getBottomSheet().addObserver(mSheetObserver);
        mBottomSheetController.get().requestShowContent(this, true);
        setVisible(true);
        mSheetTriggered = true;
    }

    /**
     * Hide the sheet and reset its model.
     * @param animation {@code true} if animation effect is to be made.
     */
    private void close(boolean animate) {
        if (!isHidden()) mBottomSheetController.get().hideContent(this, animate);
        setVisible(false);
        mBottomSheetController.get().getBottomSheet().removeObserver(mSheetObserver);
        mMediator.clear();
    }

    /**
     * @return {@code true} if the sheet is in hidden state.
     */
    private boolean isHidden() {
        return !isVisible() && getTargetOrCurrentState() == SheetState.HIDDEN;
    }

    @Override
    public boolean isPeeked() {
        if (mBottomSheetController.get() == null) return false;
        return isVisible() && getTargetOrCurrentState() == SheetState.PEEK;
    }

    private @SheetState int getTargetOrCurrentState() {
        BottomSheet sheet = mBottomSheetController.get().getBottomSheet();
        if (sheet == null) return SheetState.NONE;
        @SheetState
        int state = sheet.getTargetSheetState();
        return state != SheetState.NONE ? state : sheet.getSheetState();
    }

    /**
     * @return {@code true} if the sheet is in fully expanded state.
     */
    private boolean isExpanded() {
        return isVisible() && getTargetOrCurrentState() == SheetState.FULL;
    }

    /**
     * Set the visibility flag of the navigation sheet.
     * @param visible {@code true} if the sheet becomes visible.
     */
    private void setVisible(boolean visible) {
        mSheetVisible = visible;
    }

    /**
     * @return {@code true} if the sheet is visible.
     */
    private boolean isVisible() {
        return mSheetVisible;
    }

    // BottomSheetContent

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentView.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean isPeekStateEnabled() {
        // Makes peek state as 'not present' when bottom sheet is in expanded state (i.e. animating
        // from expanded to close state). It avoids the sheet animating in two distinct steps, which
        // looks awkward.
        return !mBottomSheetController.get().getBottomSheet().isSheetOpen();
    }

    @Override
    public boolean wrapContentEnabled() {
        return true;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return true;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.overscroll_navigation_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false : "No half state exists. Should not be invoked.";
        return -1;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.overscroll_navigation_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.overscroll_navigation_sheet_closed;
    }
}

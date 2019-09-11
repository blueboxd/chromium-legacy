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
import org.chromium.content_public.browser.NavigationHistory;
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
    // with a long swipe. This ensures fling gestures from edge won't invoke the  sheet.
    private final static int LONG_SWIPE_HOLD_DELAY_MS = 50;

    // Amount of time to hold the finger still to trigger navigation bottom sheet
    // with a short swipe.
    private final static int SHORT_SWIPE_HOLD_DELAY_MS = 400;

    // Amount of distance to trigger navigation sheet with a long swipe.
    // Actual amount is capped so it is at most half the screen width.
    private static final int LONG_SWIPE_PEEK_THRESHOLD_DP = 224;

    // The history item count in the navigation sheet. If the count is equal or smaller,
    // the sheet skips peek state and fully expands right away.
    private static final int SKIP_PEEK_COUNT = 3;

    // Delta for touch events that can happen even when users doesn't intend to move
    // his finger. Any delta smaller than (or equal to) than this are ignored.
    private static final float DELTA_IGNORE = 2.f;

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
    private final Runnable mOpenSheetRunnable;
    private final float mLongSwipePeekThreshold;

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
        mOpenSheetRunnable = () -> {
            if (isHidden()) openSheet();
        };
        mLongSwipePeekThreshold = Math.min(
                context.getResources().getDisplayMetrics().density * LONG_SWIPE_PEEK_THRESHOLD_DP,
                parent.getWidth() / 2);
    }

    // Transition to either peeked or expanded state.
    private void openSheet() {
        NavigationHistory history = mDelegate.getHistory(mForward);
        mMediator.populateEntries(history);
        mBottomSheetController.get().requestShowContent(this, true);
        mBottomSheetController.get().getBottomSheet().addObserver(mSheetObserver);
        mSheetTriggered = true;
        if (history.getEntryCount() <= SKIP_PEEK_COUNT) expandSheet();
    }

    private void expandSheet() {
        mBottomSheetController.get().expandSheet();
        GestureNavMetrics.recordHistogram("GestureNavigation.Sheet.Viewed", mForward);
    }

    // NavigationSheet

    @Override
    public void start(boolean forward, boolean showCloseIndicator) {
        if (mBottomSheetController.get() == null) return;
        mForward = forward;
        mShowCloseIndicator = showCloseIndicator;
        mSheetTriggered = false;
    }

    @Override
    public void onScroll(float delta, float overscroll, boolean willNavigate) {
        if (mBottomSheetController.get() == null) return;
        if (mShowCloseIndicator) return;
        if (overscroll > mLongSwipePeekThreshold) {
            triggerSheetWithSwipeAndHold(delta, LONG_SWIPE_HOLD_DELAY_MS);
        } else if (willNavigate) {
            triggerSheetWithSwipeAndHold(delta, SHORT_SWIPE_HOLD_DELAY_MS);
        } else if (isPeeked()) {
            close(true);
        } else {
            mHandler.removeCallbacks(mOpenSheetRunnable);
        }
    }

    private void triggerSheetWithSwipeAndHold(float delta, long delay) {
        if (isHidden() && Math.abs(delta) > DELTA_IGNORE) {
            mHandler.removeCallbacks(mOpenSheetRunnable);
            mHandler.postDelayed(mOpenSheetRunnable, delay);
        }
    }

    @Override
    public void release() {
        if (mBottomSheetController.get() == null) return;
        mHandler.removeCallbacks(mOpenSheetRunnable);
        if (mSheetTriggered) {
            GestureNavMetrics.recordHistogram("GestureNavigation.Sheet.Peeked", mForward);
        }

        // Show navigation sheet if released at peek state.
        if (isPeeked()) expandSheet();
    }

    /**
     * Hide the sheet and reset its model.
     * @param animation {@code true} if animation effect is to be made.
     */
    private void close(boolean animate) {
        if (!isHidden()) mBottomSheetController.get().hideContent(this, animate);
        mBottomSheetController.get().getBottomSheet().removeObserver(mSheetObserver);
        mMediator.clear();
    }

    @Override
    public boolean isHidden() {
        if (mBottomSheetController.get() == null) return true;
        return getTargetOrCurrentState() == SheetState.HIDDEN;
    }

    /**
     * @return {@code true} if the sheet is in peeked state.
     */
    private boolean isPeeked() {
        if (mBottomSheetController.get() == null) return false;
        return getTargetOrCurrentState() == SheetState.PEEK;
    }

    private @SheetState int getTargetOrCurrentState() {
        BottomSheet sheet = mBottomSheetController.get().getBottomSheet();
        if (sheet == null) return SheetState.NONE;
        @SheetState
        int state = sheet.getTargetSheetState();
        return state != SheetState.NONE ? state : sheet.getSheetState();
    }

    @Override
    public boolean isExpanded() {
        if (mBottomSheetController.get() == null) return false;
        int state = getTargetOrCurrentState();
        return state == SheetState.HALF || state == SheetState.FULL;
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

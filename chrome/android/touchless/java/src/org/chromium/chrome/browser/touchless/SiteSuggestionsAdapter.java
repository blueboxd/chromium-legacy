// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ASYNC_FOCUS_DELEGATE;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.CURRENT_INDEX_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.INITIAL_INDEX_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ITEM_COUNT_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.ON_FOCUS_CALLBACK;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.REMOVAL_KEY;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.SHOULD_FOCUS_VIEW;
import static org.chromium.chrome.browser.touchless.SiteSuggestionsCoordinator.SUGGESTIONS_KEY;

import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.ContextMenu;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Recycler view adapter and view binder for Touchless Suggestions carousel.
 *
 * Allows for an almost-infinite side-scrolling carousel with snapping focus in the center if
 * there are 2 or more items; does not scroll if there is 1 or fewer items.
 *
 * Focus changes on containing items will cause the passed-in title view to change.
 */
class SiteSuggestionsAdapter extends ForwardingListObservable<PropertyKey>
        implements RecyclerViewAdapter.Delegate<
                           SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, PropertyKey>,
                   PropertyObservable.PropertyObserver<PropertyKey> {
    @IntDef({ViewType.ALL_APPS_TYPE, ViewType.SUGGESTION_TYPE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int ALL_APPS_TYPE = 0;
        int SUGGESTION_TYPE = 1;
    }

    private class SiteSuggestionInteractionDelegate
            implements TouchlessContextMenuManager.Delegate, View.OnCreateContextMenuListener {
        private PropertyModel mSuggestion;

        SiteSuggestionInteractionDelegate(PropertyModel model) {
            mSuggestion = model;
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContextMenuManager.createContextMenu(menu, v, this);
        }

        @Override
        public void openItem(int windowDisposition) {
            mNavDelegate.navigateToSuggestionUrl(
                    windowDisposition, mSuggestion.get(SiteSuggestionModel.URL_KEY));
        }

        @Override
        public void removeItem() {
            // Notify about removal.
            mModel.set(REMOVAL_KEY, mSuggestion);
            // Force-trigger rebind of current_index to update text.
            onPropertyChanged(mModel, CURRENT_INDEX_KEY);
        }

        @Override
        public String getUrl() {
            return mSuggestion.get(SiteSuggestionModel.URL_KEY);
        }

        @Override
        public String getContextMenuTitle() {
            return mSuggestion.get(SiteSuggestionModel.TITLE_KEY);
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            return menuItemId == ContextMenuManager.ContextMenuItemId.SEARCH
                    || menuItemId == ContextMenuManager.ContextMenuItemId.REMOVE;
        }

        @Override
        public void onContextMenuCreated() {}

        @Override
        public String getTitle() {
            return mSuggestion.get(SiteSuggestionModel.TITLE_KEY);
        }

        @Override
        public Bitmap getIconBitmap() {
            if (mSuggestion.get(SiteSuggestionModel.ICON_KEY) == null) {
                return mSuggestion.get(SiteSuggestionModel.DEFAULT_ICON_KEY);
            }
            return mSuggestion.get(SiteSuggestionModel.ICON_KEY);
        }
    }

    private PropertyModel mModel;
    private SuggestionsNavigationDelegate mNavDelegate;
    private ContextMenuManager mContextMenuManager;
    private SiteSuggestionsLayoutManager mLayoutManager;
    private TextView mTitleView;

    /**
     * @param model the main property model coming from {@link SiteSuggestionsCoordinator}.
     * @param navigationDelegate delegate for navigation controls
     * @param contextMenuManager handles context menu creation
     * @param layoutManager the layout manager controlling this recyclerview and adapter
     * @param titleView the view to update site title when focus changes.
     */
    SiteSuggestionsAdapter(PropertyModel model, SuggestionsNavigationDelegate navigationDelegate,
            ContextMenuManager contextMenuManager, SiteSuggestionsLayoutManager layoutManager,
            TextView titleView) {
        mModel = model;
        mNavDelegate = navigationDelegate;
        mContextMenuManager = contextMenuManager;
        mLayoutManager = layoutManager;
        mTitleView = titleView;

        mModel.get(SUGGESTIONS_KEY).addObserver(this);
        mModel.addObserver(this);

        // Initialize the titleView text.
        onPropertyChanged(mModel, CURRENT_INDEX_KEY);
    }

    @Override
    public int getItemCount() {
        if (mModel.get(SUGGESTIONS_KEY).size() > 0) {
            return Integer.MAX_VALUE;
        }
        return 1;
    }

    @Override
    public int getItemViewType(int position) {
        int itemCount = mModel.get(ITEM_COUNT_KEY);
        if (itemCount == 1 || position % itemCount == 0) return ViewType.ALL_APPS_TYPE;
        return ViewType.SUGGESTION_TYPE;
    }

    @Override
    public void onBindViewHolder(SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder holder,
            int position, PropertyKey payload) {
        SiteSuggestionsTileView tile = (SiteSuggestionsTileView) holder.itemView;
        // Updates focus change listener.
        tile.setOnFocusChangeListener((View v, boolean hasFocus) -> {
            if (hasFocus) {
                mModel.set(CURRENT_INDEX_KEY, position);
                if (mModel.get(ON_FOCUS_CALLBACK) != null) {
                    mModel.get(ON_FOCUS_CALLBACK).run();
                }
            }
        });

        if (holder.getItemViewType() == ViewType.ALL_APPS_TYPE) {
            tile.setIconDrawable(VectorDrawableCompat.create(tile.getResources(),
                    R.drawable.ic_apps_blue_24dp, tile.getContext().getTheme()));
            // If explore sites, clicks navigate to the Explore URL.
            tile.setOnClickListener(
                    (view)
                            -> mNavDelegate.navigateToSuggestionUrl(
                                    WindowOpenDisposition.CURRENT_TAB, UrlConstants.EXPLORE_URL));
            ContextMenuManager.registerViewForTouchlessContextMenu(
                    tile, new ContextMenuManager.EmptyDelegate() {
                        @Override
                        public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
                            return menuItemId == ContextMenuManager.ContextMenuItemId.SEARCH;
                        }
                    });
            tile.setContentDescription(tile.getResources().getString(R.string.ntp_all_apps));
        } else if (holder.getItemViewType() == ViewType.SUGGESTION_TYPE) {
            // If site suggestion, attach context menu handler; clicks navigate to site url.
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            // Subtract 1 from position % MAX_TILES to account for "all apps" taking up one space.
            PropertyModel item = mModel.get(SUGGESTIONS_KEY).get((position % itemCount) - 1);
            // Only update the icon for icon updates.
            if (payload == SiteSuggestionModel.ICON_KEY) {
                tile.updateIcon(item.get(SiteSuggestionModel.ICON_KEY),
                        item.get(SiteSuggestionModel.DEFAULT_ICON_KEY));
            } else {
                tile.updateIcon(item.get(SiteSuggestionModel.ICON_KEY),
                        item.get(SiteSuggestionModel.DEFAULT_ICON_KEY));

                SiteSuggestionInteractionDelegate interactionDelegate =
                        new SiteSuggestionInteractionDelegate(item);

                tile.setOnClickListener(
                        (View v)
                                -> interactionDelegate.openItem(WindowOpenDisposition.CURRENT_TAB));

                tile.setOnCreateContextMenuListener(interactionDelegate);
                ContextMenuManager.registerViewForTouchlessContextMenu(tile, interactionDelegate);
                tile.setContentDescription(item.get(SiteSuggestionModel.TITLE_KEY));
            }
        }
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey == CURRENT_INDEX_KEY) {
            // When the current index changes, we want to update the title.
            int position = mModel.get(CURRENT_INDEX_KEY);
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            if (itemCount == 1 || position % itemCount == 0) {
                mTitleView.setText(R.string.ntp_all_apps);
            } else {
                mTitleView.setText(mModel.get(SUGGESTIONS_KEY)
                                           .get(position % itemCount - 1)
                                           .get(SiteSuggestionModel.TITLE_KEY));
            }
        } else if (propertyKey == SHOULD_FOCUS_VIEW && mModel.get(SHOULD_FOCUS_VIEW)
                && mModel.get(ASYNC_FOCUS_DELEGATE) != null) {
            mLayoutManager.focusCenterItem();
            mModel.set(SHOULD_FOCUS_VIEW, false);
        } else if (propertyKey == INITIAL_INDEX_KEY) {
            mLayoutManager.scrollToPosition(mModel.get(INITIAL_INDEX_KEY));
        }
    }

    @Override
    public void notifyItemRangeInserted(int index, int count) {
        if (mModel.get(SUGGESTIONS_KEY).size() == 1) {
            // When we just added something for the first time,
            // we would change from non-scrolling to infinite-scrolling.
            super.notifyItemRangeInserted(1, Integer.MAX_VALUE - 1);
        } else {
            // Otherwise, rebind the visible spectrum.
            // Account for edge conditions so we don't crash in the impossible case.
            int start = mLayoutManager.findFirstVisibleItemPosition();
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            int newCount = Integer.MAX_VALUE - 1 - start >= (itemCount * 2)
                    ? itemCount * 2
                    : Integer.MAX_VALUE - 1 - start;
            super.notifyItemRangeChanged(start, newCount, null);
        }
    }

    @Override
    public void notifyItemRangeRemoved(int index, int count) {
        if (mModel.get(SUGGESTIONS_KEY).size() == 0) {
            // When we removed the last item in the model, we would go from infinite scroll
            // back to non-scrolling. Notify Recyclerview to remove everything.
            super.notifyItemRangeRemoved(1, Integer.MAX_VALUE - 1);
        } else {
            // Otherwise, rebind the visible spectrum.
            // Account for edge conditions so we don't crash in the impossible case.
            int start = mLayoutManager.findFirstVisibleItemPosition();
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            int newCount = Integer.MAX_VALUE - 1 - start >= (itemCount * 2)
                    ? itemCount * 2
                    : Integer.MAX_VALUE - 1 - start;
            super.notifyItemRangeChanged(start, newCount, null);
        }
    }

    @Override
    public void notifyItemRangeChanged(int index, int count, @Nullable PropertyKey payload) {
        if (mModel.get(SUGGESTIONS_KEY).size() == 0) {
            // If itemCount is 1, then notify super.
            // This should only happen if "All apps" icon has changed in some way and we aren't
            // infinite-scrolling.
            super.notifyItemRangeChanged(index, count, payload);
        } else {
            // Otherwise, rebind the visible spectrum.
            // Account for edge conditions so we don't crash in the impossible case.
            int start = mLayoutManager.findFirstVisibleItemPosition();
            int itemCount = mModel.get(ITEM_COUNT_KEY);
            int newCount = Integer.MAX_VALUE - 1 - start >= (itemCount * 2)
                    ? itemCount * 2
                    : Integer.MAX_VALUE - 1 - start;
            super.notifyItemRangeChanged(start, newCount, payload);
        }
    }
}

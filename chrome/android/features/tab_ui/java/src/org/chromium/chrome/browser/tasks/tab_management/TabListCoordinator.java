// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Coordinator for showing UI for a list of tabs. Can be used in GRID or STRIP modes.
 */
public class TabListCoordinator implements Destroyable {
    /**
     * Modes of showing the list of tabs.
     *
     * NOTE: CAROUSEL mode currently uses a fixed height and card width set in dimens.xml with names
     *  tab_carousel_height and tab_carousel_card_width.
     *
     *  STRIP and GRID modes will have height equal to that of the container view.
     * */
    @IntDef({TabListMode.GRID, TabListMode.STRIP, TabListMode.CAROUSEL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMode {
        int GRID = 0;
        int STRIP = 1;
        int CAROUSEL = 2;
        int NUM_ENTRIES = 3;
    }

    static final int GRID_LAYOUT_SPAN_COUNT_PORTRAIT = 2;
    static final int GRID_LAYOUT_SPAN_COUNT_LANDSCAPE = 3;
    private final SimpleRecyclerViewMcpBase mModelChangeProcessor;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final @TabListMode int mMode;
    private final Rect mThumbnailLocationOfCurrentTab = new Rect();
    private final SimpleRecyclerViewMcpBase
            .ItemViewTypeCallback<PropertyModel> mGridDefaultItemViewTypeCallback =
            (item) -> TabGridViewHolder.TabGridViewItemType.CLOSABLE_TAB;

    /**
     * Construct a coordinator for UI that shows a list of tabs.
     * @param mode Modes of showing the list of tabs. Can be used in GRID or STRIP.
     * @param context The context to use for accessing {@link android.content.res.Resources}.
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                              the tabs concerned.
     * @param thumbnailProvider Provider to provide screenshot related details.
     * @param titleProvider Provider for a given tab's title.
     * @param actionOnRelatedTabs Whether tab-related actions should be operated on all related
     *                            tabs.
     * @param createGroupButtonProvider {@link TabListMediator.CreateGroupButtonProvider}
     *         to provide "Create group" button.
     * @param gridCardOnClickListenerProvider Provides the onClickListener for opening dialog when
     *                                        click on a grid card.
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param itemViewTypeCallback Callback that returns the view type for each item in the list.
     * @param selectionDelegateProvider Provider to provide selected Tabs for a selectable tab list.
     *                                  It's NULL when selection is not possible.
     * @param parentView {@link ViewGroup} The root view of the UI.
     * @param dynamicResourceLoader The {@link DynamicResourceLoader} to register dynamic UI
     *                              resource for compositor layer animation.
     * @param attachToParent Whether the UI should attach to root view.
     * @param componentName A unique string uses to identify different components for UMA recording.
     *                      Recommended to use the class name or make sure the string is unique
     *                      through actions.xml file.
     */
    TabListCoordinator(@TabListMode int mode, Context context, TabModelSelector tabModelSelector,
            @Nullable TabListMediator.ThumbnailProvider thumbnailProvider,
            @Nullable TabListMediator.TitleProvider titleProvider, boolean actionOnRelatedTabs,
            @Nullable TabListMediator.CreateGroupButtonProvider createGroupButtonProvider,
            @Nullable TabListMediator
                    .GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabListMediator.TabGridDialogHandler dialogHandler,
            SimpleRecyclerViewMcpBase.ItemViewTypeCallback<PropertyModel> itemViewTypeCallback,
            @Nullable TabListMediator.SelectionDelegateProvider selectionDelegateProvider,
            @NonNull ViewGroup parentView, @Nullable DynamicResourceLoader dynamicResourceLoader,
            boolean attachToParent, String componentName) {
        TabListModel tabListModel = new TabListModel();
        mMode = mode;

        RecyclerViewAdapter adapter;
        if (mMode == TabListMode.GRID || mMode == TabListMode.CAROUSEL) {
            if (itemViewTypeCallback == null) {
                itemViewTypeCallback = mGridDefaultItemViewTypeCallback;
            }

            SimpleRecyclerViewMcpBase<PropertyModel, TabGridViewHolder, PropertyKey> mcp =
                    new SimpleRecyclerViewMcpBase<PropertyModel, TabGridViewHolder, PropertyKey>(
                            itemViewTypeCallback, TabGridViewBinder::onBindViewHolder,
                            tabListModel) {
                        @Override
                        public void onViewRecycled(TabGridViewHolder viewHolder) {
                            viewHolder.resetThumbnail();
                        }
                    };
            if (mode == TabListMode.CAROUSEL) {
                adapter = new RecyclerViewAdapter<>(mcp, TabCarouselViewHolder::create);
            } else {
                adapter = new RecyclerViewAdapter<>(mcp, TabGridViewHolder::create);
            }
            mModelChangeProcessor = mcp;
        } else if (mMode == TabListMode.STRIP) {
            SimpleRecyclerViewMcpBase<PropertyModel, TabStripViewHolder, PropertyKey> mcp =
                    new SimpleRecyclerViewMcpBase<>(
                            null, TabStripViewBinder::onBindViewHolder, tabListModel);
            adapter = new RecyclerViewAdapter<>(mcp, TabStripViewHolder::create);
            mModelChangeProcessor = mcp;
        } else {
            throw new IllegalArgumentException(
                    "Attempting to create a tab list UI with invalid mode");
        }

        if (!attachToParent) {
            mRecyclerView = (TabListRecyclerView) LayoutInflater.from(context).inflate(
                    R.layout.tab_list_recycler_view_layout, parentView, false);
        } else {
            LayoutInflater.from(context).inflate(
                    R.layout.tab_list_recycler_view_layout, parentView, true);
            mRecyclerView = parentView.findViewById(R.id.tab_list_view);
        }

        if (mode == TabListMode.CAROUSEL) {
            // TODO(mattsimmons): Remove this height and let the parent determine the correct
            //  height. This can be done once the width is dynamic as well in
            //  TabCarouselViewHolder.
            mRecyclerView.getLayoutParams().height =
                    context.getResources().getDimensionPixelSize(R.dimen.tab_carousel_height);
        }

        mRecyclerView.setAdapter(adapter);
        mRecyclerView.setHasFixedSize(true);

        if (dynamicResourceLoader != null) {
            mRecyclerView.createDynamicView(dynamicResourceLoader);
        }

        TabListFaviconProvider tabListFaviconProvider =
                new TabListFaviconProvider(context, Profile.getLastUsedProfile());

        mMediator = new TabListMediator(context, tabListModel, tabModelSelector, thumbnailProvider,
                titleProvider, tabListFaviconProvider, actionOnRelatedTabs,
                createGroupButtonProvider, selectionDelegateProvider,
                gridCardOnClickListenerProvider, dialogHandler, componentName);

        if (mMode == TabListMode.GRID) {
            mRecyclerView.setLayoutManager(
                    new GridLayoutManager(context, GRID_LAYOUT_SPAN_COUNT_PORTRAIT));
            mMediator.updateSpanCountForOrientation(
                    (GridLayoutManager) mRecyclerView.getLayoutManager(),
                    context.getResources().getConfiguration().orientation);
        } else if (mMode == TabListMode.STRIP || mMode == TabListMode.CAROUSEL) {
            mRecyclerView.setLayoutManager(
                    new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        }

        if (mMode == TabListMode.GRID && selectionDelegateProvider == null) {
            ItemTouchHelper touchHelper = new ItemTouchHelper(mMediator.getItemTouchHelperCallback(
                    context.getResources().getDimension(R.dimen.swipe_to_dismiss_threshold),
                    context.getResources().getDimension(R.dimen.tab_grid_merge_threshold),
                    context.getResources().getDimension(R.dimen.bottom_sheet_peek_height),
                    tabModelSelector.getCurrentModel().getProfile()));
            touchHelper.attachToRecyclerView(mRecyclerView);
            mMediator.registerOrientationListener(
                    (GridLayoutManager) mRecyclerView.getLayoutManager());

            // TODO(crbug.com/964406): unregister the listener when we don't need it.
            mRecyclerView.getViewTreeObserver().addOnGlobalLayoutListener(
                    this::updateThumbnailLocation);
        }
    }

    @NonNull
    Rect getThumbnailLocationOfCurrentTab() {
        // TODO(crbug.com/964406): calculate the location before the real one is ready.
        return mThumbnailLocationOfCurrentTab;
    }

    @NonNull
    Rect getRecyclerViewLocation() {
        Rect recyclerViewRect = new Rect();
        mRecyclerView.getGlobalVisibleRect(recyclerViewRect);
        return recyclerViewRect;
    }

    /**
     * Update the location of the selected thumbnail.
     * @return Whether a valid {@link Rect} is obtained.
     */
    boolean updateThumbnailLocation() {
        Rect rect = mRecyclerView.getRectOfCurrentThumbnail(
                mMediator.indexOfSelected(), mMediator.selectedTabId());
        if (rect == null) return false;
        mThumbnailLocationOfCurrentTab.set(rect);
        return true;
    }

    /**
     * @return The container {@link android.support.v7.widget.RecyclerView} that is showing the
     *         tab list UI.
     */
    public TabListRecyclerView getContainerView() {
        return mRecyclerView;
    }

    /**
     * @see TabListMediator#resetWithListOfTabs(List, boolean)
     */
    boolean resetWithListOfTabs(@Nullable List<Tab> tabs, boolean quickMode) {
        if (mMode == TabListMode.STRIP && tabs != null && tabs.size() > 1) {
            TabGroupUtils.maybeShowIPH(
                    FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE, mRecyclerView);
        }
        return mMediator.resetWithListOfTabs(tabs, quickMode);
    }

    boolean resetWithListOfTabs(@Nullable List<Tab> tabs) {
        return resetWithListOfTabs(tabs, false);
    }

    void softCleanup() {
        mMediator.softCleanup();
    }

    void prepareOverview() {
        mRecyclerView.prepareOverview();
        mMediator.prepareOverview();
    }

    void postHiding() {
        mRecyclerView.postHiding();
        mMediator.postHiding();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mMediator.destroy();
    }

    int getResourceId() {
        return mRecyclerView.getResourceId();
    }

    long getLastDirtyTimeForTesting() {
        return mRecyclerView.getLastDirtyTimeForTesting();
    }
}

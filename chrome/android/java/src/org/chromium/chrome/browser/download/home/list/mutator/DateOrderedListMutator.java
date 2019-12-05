// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItemModel;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;

/**
 * A class responsible for turning a {@link Collection} of {@link OfflineItem}s into a list meant
 * to be displayed in the download home UI.  This list has the following properties:
 * - Sorted, according to a given {@link Comparator}.
 * - Separated by labels, such as date header, pagination header or domain header.
 * - Converts changes in the form of {@link Collection}s to delta changes on the list.
 */
public class DateOrderedListMutator implements OfflineItemFilterObserver {
    /**
     * Sorts a list of {@link ListItem}.
     */
    public interface Sorter {
        /**
         * Sorts a given list as per display requirements.
         * @param list The input list to be sorted.
         * @return The sorted output list.
         */
        ArrayList<ListItem> sort(ArrayList<ListItem> list);
    }

    /**
     * Given a sorted list of offline items, generates a list of {@link ListItem} with
     * appropriate labels inserted at the right positions as per the display requirements.
     */
    public interface LabelAdder {
        /**
         * Inserts various labels to the given list as per display requirements.
         * @param sortedList The input list to be displayed.
         * @return The output list to be displayed on screen.
         */
        List<ListItem> addLabels(List<ListItem> sortedList);
    }

    private final OfflineItemFilterSource mSource;
    private final JustNowProvider mJustNowProvider;
    private final ListItemModel mModel;
    private final Paginator mPaginator;
    private ArrayList<ListItem> mSortedItems = new ArrayList<>();

    private Sorter mSorter;
    private LabelAdder mLabelAdder;
    private ListItemPropertySetter mPropertySetter;

    /**
     * Creates an DateOrderedList instance that will reflect {@code source}.
     * @param source The source of data for this list.
     * @param model  The model that will be the storage for the updated list.
     * @param justNowProvider The provider for Just Now section.
     * @param sorter The default sorter to use for sorting the list.
     * @param labelAdder The label adder used for producing the final list.
     * @param paginator The paginator to handle pagination.
     */
    public DateOrderedListMutator(OfflineItemFilterSource source, ListItemModel model,
            JustNowProvider justNowProvider, Sorter sorter, LabelAdder labelAdder,
            ListItemPropertySetter propertySetter, Paginator paginator) {
        mSource = source;
        mModel = model;
        mJustNowProvider = justNowProvider;
        mPropertySetter = propertySetter;
        mPaginator = paginator;
        mSource.addObserver(this);
        setMutators(sorter, labelAdder);
        onItemsAdded(mSource.getItems());
    }

    /**
     * Called when the desired sorting order has changed.
     * @param sorter The sorter to use for sorting the list.
     * @param labelAdder The label adder used for producing the final list.
     */
    public void setMutators(Sorter sorter, LabelAdder labelAdder) {
        if (mSorter == sorter && mLabelAdder == labelAdder) return;
        mSorter = sorter;
        mLabelAdder = labelAdder;
        mSortedItems = mSorter.sort(mSortedItems);
    }

    /**
     * Called to add more pages to show in the list.
     */
    public void loadMorePages() {
        mPaginator.loadMorePages();
        pushItemsToModel();
    }

    /** Called to reload the list and display. */
    public void reload() {
        pushItemsToModel();
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        for (OfflineItem offlineItem : items) {
            OfflineItemListItem listItem = new OfflineItemListItem(offlineItem);
            mSortedItems.add(listItem);
        }
        mSortedItems = mSorter.sort(mSortedItems);
        pushItemsToModel();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        for (OfflineItem itemToRemove : items) {
            for (int i = 0; i < mSortedItems.size(); i++) {
                OfflineItem offlineItem = ((OfflineItemListItem) mSortedItems.get(i)).item;
                if (itemToRemove.id.equals(offlineItem.id)) mSortedItems.remove(i);
            }
        }
        pushItemsToModel();
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        assert oldItem.id.equals(item.id);

        // If the update changed the creation time or filter type, remove and add the element to get
        // it positioned.
        if (oldItem.creationTimeMs != item.creationTimeMs || oldItem.filter != item.filter
                || mJustNowProvider.isJustNowItem(oldItem)
                        != mJustNowProvider.isJustNowItem((item))) {
            // TODO(shaktisahu): Collect UMA when this happens.
            onItemsRemoved(CollectionUtil.newArrayList(oldItem));
            onItemsAdded(CollectionUtil.newArrayList(item));
        } else {
            for (int i = 0; i < mSortedItems.size(); i++) {
                if (item.id.equals(((OfflineItemListItem) mSortedItems.get(i)).item.id)) {
                    mSortedItems.set(i, new OfflineItemListItem(item));
                }
            }
            updateModelListItem(item);
        }

        mModel.dispatchLastEvent();
    }

    private void updateModelListItem(OfflineItem item) {
        for (int i = 0; i < mModel.size(); i++) {
            ListItem listItem = mModel.get(i);
            if (!(listItem instanceof OfflineItemListItem)) continue;

            OfflineItemListItem existingItem = (OfflineItemListItem) listItem;
            if (item.id.equals(existingItem.item.id)) {
                existingItem.item = item;
                mModel.update(i, existingItem);
                break;
            }
        }
    }

    private void pushItemsToModel() {
        // TODO(shaktisahu): Add paginated list after finalizing UX.

        List<ListItem> listItems = mLabelAdder.addLabels(mSortedItems);
        mPropertySetter.setProperties(listItems);
        mModel.set(listItems);
        mModel.dispatchLastEvent();
    }
}

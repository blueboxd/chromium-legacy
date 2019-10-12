// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderListItem;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * A class responsible for turning a {@link Collection} of {@link OfflineItem}s into a list meant
 * to be displayed in the download home UI.  This list has the following properties:
 * - Sorted.
 * - Separated by date headers for each individual day.
 * - Converts changes in the form of {@link Collection}s to delta changes on the list.
 *
 * TODO(dtrainor): This should be optimized in the near future.  There are a few key things that can
 * be changed:
 * - Do a single iterating across each list to merge/unmerge them.  This requires sorting and
 *   tracking the current position across both as iterating (see {@link #onItemsRemoved(Collection)}
 *   for an example since that is close to doing what we want - minus the contains() call).
 */
class DateOrderedListMutator implements OfflineItemFilterObserver {
    private static final Date JUST_NOW_DATE = new Date(Long.MAX_VALUE);
    private final DownloadManagerUiConfig mConfig;
    private final JustNowProvider mJustNowProvider;
    private final ListItemModel mModel;

    private final Map<Date, DateGroup> mDateGroups =
            new TreeMap<>((lhs, rhs) -> { return rhs.compareTo(lhs); });

    // Whether we shouldn't have any headers. Meant to be used for prefetch tab.
    private boolean mHideAllHeaders;

    /**
     * Creates an DateOrderedList instance that will reflect {@code source}.
     * @param source The source of data for this list.
     * @param model  The model that will be the storage for the updated list.
     * @param config The {@link DownloadManagerUiConfig}.
     * @param justNowProvider The provider for Just Now section.
     */
    public DateOrderedListMutator(OfflineItemFilterSource source, ListItemModel model,
            DownloadManagerUiConfig config, JustNowProvider justNowProvider) {
        mModel = model;
        mConfig = config;
        mJustNowProvider = justNowProvider;
        source.addObserver(this);
        onItemsAdded(source.getItems());
    }

    /**
     * Called when the selected tab or chip has changed.
     * @param filter The currently selected filter type.
     */
    public void onFilterTypeSelected(@Filters.FilterType int filter) {
        mHideAllHeaders = filter == Filters.FilterType.PREFETCHED;
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        for (OfflineItem item : items) {
            addOrUpdateItemToDateGroups(item);
        }

        pushItemsToModel();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        for (OfflineItem item : items) {
            Date date = getSectionDateFromOfflineItem(item);
            DateGroup dateGroup = mDateGroups.get(date);
            if (dateGroup == null) continue;

            dateGroup.removeItem(item);
            if (dateGroup.sections.isEmpty()) {
                mDateGroups.remove(date);
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
                || shouldShowInJustNowSection(oldItem) != shouldShowInJustNowSection((item))) {
            // TODO(shaktisahu): Collect UMA when this happens.
            onItemsRemoved(CollectionUtil.newArrayList(oldItem));
            onItemsAdded(CollectionUtil.newArrayList(item));
        } else {
            addOrUpdateItemToDateGroups(item);

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

        mModel.dispatchLastEvent();
    }

    private void addOrUpdateItemToDateGroups(OfflineItem item) {
        Date date = getSectionDateFromOfflineItem(item);
        DateGroup dateGroup = mDateGroups.get(date);
        if (dateGroup == null) {
            dateGroup = new DateGroup();
            mDateGroups.put(date, dateGroup);
        }
        dateGroup.addOrUpdateItem(item);
    }

    // Flattens out the hierarchical data and adds items to the model in the order they should be
    // displayed. Date headers and section headers are added wherever necessary. The existing items
    // in the model are replaced by the new set of items computed.
    private void pushItemsToModel() {
        List<ListItem> listItems = new ArrayList<>();
        int dateIndex = 0;
        for (Date date : mDateGroups.keySet()) {
            DateGroup dateGroup = mDateGroups.get(date);
            int sectionIndex = 0;

            // For each section.
            for (Integer filter : dateGroup.sections.keySet()) {
                Section section = dateGroup.sections.get(filter);

                // Add a section header.
                if (!mHideAllHeaders && sectionIndex == 0) {
                    SectionHeaderListItem sectionHeaderItem = new SectionHeaderListItem(
                            date.getTime(), date.equals(JUST_NOW_DATE) /* isJustNow */,
                            sectionIndex == 0 && dateIndex > 0 /* showDivider */);
                    listItems.add(sectionHeaderItem);
                }

                // Add the items in the section.
                for (OfflineItem offlineItem : section.items) {
                    OfflineItemListItem item = new OfflineItemListItem(offlineItem);
                    if (mConfig.supportFullWidthImages && section.items.size() == 1
                            && offlineItem.filter == OfflineItemFilter.IMAGE) {
                        item.spanFullWidth = true;
                    }
                    listItems.add(item);
                }

                sectionIndex++;
            }

            dateIndex++;
        }

        mModel.set(listItems);
        mModel.dispatchLastEvent();
    }

    private Date getSectionDateFromOfflineItem(OfflineItem offlineItem) {
        return shouldShowInJustNowSection(offlineItem)
                ? JUST_NOW_DATE
                : CalendarUtils.getStartOfDay(offlineItem.creationTimeMs).getTime();
    }

    private boolean isShowingInJustNowSection(OfflineItem item) {
        DateGroup justNowGroup = mDateGroups.get(JUST_NOW_DATE);
        return justNowGroup != null && justNowGroup.contains(item.id);
    }

    private boolean shouldShowInJustNowSection(OfflineItem item) {
        return mJustNowProvider.isJustNowItem(item) || isShowingInJustNowSection(item);
    }

    /** Represents a group of items which were downloaded on the same day. */
    private static class DateGroup {
        /**
         * The list of sections for the day. The ordering is done in the same order as {@code
         * Filters.FilterType}.
         */
        public Map<Integer, Section> sections = new TreeMap<>((lhs, rhs) -> {
            return ListUtils.compareFilterTypesTo(
                    Filters.fromOfflineItem(lhs), Filters.fromOfflineItem(rhs));
        });

        public void addOrUpdateItem(OfflineItem item) {
            Section section = sections.get(item.filter);
            if (section == null) {
                section = new Section();
                sections.put(item.filter, section);
            }
            section.addOrUpdateItem(item);
        }

        public void removeItem(OfflineItem item) {
            Section section = sections.get(item.filter);
            if (section == null) return;

            section.removeItem(item);
            if (section.items.isEmpty()) {
                sections.remove(item.filter);
            }
        }

        public boolean contains(ContentId id) {
            for (Section section : sections.values()) {
                for (OfflineItem item : section.items) {
                    if (item.id.equals(id)) return true;
                }
            }
            return false;
        }
    }

    /** Represents a group of items having the same filter type. */
    private static class Section {
        /**
         * The list of items in a section for a day. Ordered by descending creation time then
         * namespace and finally by id.
         */
        public Set<OfflineItem> items = new TreeSet<>((lhs, rhs) -> {
            int comparison = Long.compare(rhs.creationTimeMs, lhs.creationTimeMs);
            if (comparison != 0) return comparison;
            comparison = lhs.id.namespace.compareTo(rhs.id.namespace);
            if (comparison != 0) return comparison;
            return lhs.id.id.compareTo(rhs.id.id);
        });

        public void addOrUpdateItem(OfflineItem item) {
            items.remove(item);
            items.add(item);
        }

        public void removeItem(OfflineItem item) {
            items.remove(item);
        }
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.list.CalendarUtils;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Given a list of {@link ListItem}, returns a list that has date headers for each date.
 * Also adds Just Now header for recently completed items. Note that this class must be called on
 * the list before adding any other labels such as card header/footer/pagination etc.
 */
public class DateLabelAdder implements ListConsumer {
    private final DownloadManagerUiConfig mConfig;
    @Nullable
    private final JustNowProvider mJustNowProvider;
    private ListConsumer mListConsumer;

    public DateLabelAdder(
            DownloadManagerUiConfig config, @Nullable JustNowProvider justNowProvider) {
        mConfig = config;
        mJustNowProvider = justNowProvider;
    }

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(addLabels(inputList));
    }

    private List<ListItem> addLabels(List<ListItem> sortedList) {
        List<ListItem> listItems = new ArrayList<>();
        OfflineItem previousItem = null;
        for (int i = 0; i < sortedList.size(); i++) {
            ListItem listItem = sortedList.get(i);
            if (!(listItem instanceof OfflineItemListItem)) continue;

            OfflineItem offlineItem = ((OfflineItemListItem) listItem).item;
            boolean startOfNewDay = startOfNewDay(offlineItem, previousItem);
            boolean isJustNow =
                    mJustNowProvider != null && mJustNowProvider.isJustNowItem(offlineItem);
            if (isJustNow) startOfNewDay = false;
            if (startOfNewDay || justNowSectionsDiffer(offlineItem, previousItem)) {
                addDateHeader(listItems, offlineItem, i);
            }

            listItems.add(listItem);
            previousItem = offlineItem;
        }

        return listItems;
    }

    private void addDateHeader(List<ListItem> listItems, OfflineItem currentItem, int index) {
        Date day = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        boolean isJustNow = mJustNowProvider != null && mJustNowProvider.isJustNowItem(currentItem);
        ListItem.SectionHeaderListItem sectionHeaderItem =
                new ListItem.SectionHeaderListItem(day.getTime(), isJustNow, index != 0);
        listItems.add(sectionHeaderItem);
    }

    private static boolean startOfNewDay(
            OfflineItem currentItem, @Nullable OfflineItem previousItem) {
        Date currentDay = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        Date previousDay = previousItem == null
                ? null
                : CalendarUtils.getStartOfDay(previousItem.creationTimeMs).getTime();
        return !currentDay.equals(previousDay);
    }

    private boolean justNowSectionsDiffer(
            OfflineItem currentItem, @Nullable OfflineItem previousItem) {
        if (mJustNowProvider == null) return false;
        if (currentItem == null || previousItem == null) return true;
        return mJustNowProvider.isJustNowItem(currentItem)
                != mJustNowProvider.isJustNowItem(previousItem);
    }
}

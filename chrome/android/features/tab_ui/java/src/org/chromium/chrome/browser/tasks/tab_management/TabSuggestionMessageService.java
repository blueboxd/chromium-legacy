// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.DISMISSED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsObserver;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * One of the concrete {@link MessageService} that only serve {@link MessageType.TAB_SUGGESTION}.
 */
public class TabSuggestionMessageService extends MessageService implements TabSuggestionsObserver {
    static final int CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD = 1;
    static final int GROUP_SUGGESTION_ACTION_ENABLING_THRESHOLD = 2;
    private static boolean sSuggestionAvailableForTesting;

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    public class TabSuggestionMessageData implements MessageData {
        private final TabSuggestion mTabSuggestion;
        private final MessageCardView.ReviewActionProvider mReviewActionProvider;
        private final MessageCardView.DismissActionProvider mDismissActionProvider;

        public TabSuggestionMessageData(TabSuggestion tabSuggestion,
                MessageCardView.ReviewActionProvider reviewActionProvider,
                MessageCardView.DismissActionProvider dismissActionProvider) {
            mTabSuggestion = tabSuggestion;
            mReviewActionProvider = reviewActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /**
         * @return The suggested tabs count.
         */
        public int getSize() {
            return mTabSuggestion.getTabsInfo().size();
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return mReviewActionProvider;
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;

    private TabSuggestion mCurrentBestTabSuggestion;
    private Callback<TabSuggestionFeedback> mCurrentTabSuggestionFeedback;

    public TabSuggestionMessageService(Context context, TabModelSelector tabModelSelector,
            TabSelectionEditorCoordinator
                    .TabSelectionEditorController tabSelectionEditorController) {
        super(MessageType.TAB_SUGGESTION);
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mTabSelectionEditorController = tabSelectionEditorController;
    }

    @VisibleForTesting
    void review() {
        assert mCurrentBestTabSuggestion != null;

        mTabSelectionEditorController.configureToolbar(getActionString(mCurrentBestTabSuggestion),
                getActionProvider(mCurrentBestTabSuggestion),
                getEnablingThreshold(mCurrentBestTabSuggestion), getNavigationProvider());

        mTabSelectionEditorController.show(
                getTabList(), mCurrentBestTabSuggestion.getTabsInfo().size());
    }

    private String getActionString(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return mContext.getString(R.string.tab_suggestion_close_tab_action_button);
            case TabSuggestion.TabSuggestionAction.GROUP:
                return mContext.getString(R.string.tab_selection_editor_group);
            default:
                assert false;
        }
        return null;
    }

    private int getEnablingThreshold(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD;
            case TabSuggestion.TabSuggestionAction.GROUP:
                return GROUP_SUGGESTION_ACTION_ENABLING_THRESHOLD;
            default:
                assert false;
        }
        return -1;
    }

    @VisibleForTesting
    TabSelectionEditorActionProvider getActionProvider(TabSuggestion tabSuggestion) {
        int action;
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                action = TabSelectionEditorActionProvider.TabSelectionEditorAction.CLOSE;
                break;
            case TabSuggestion.TabSuggestionAction.GROUP:
                action = TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP;
                break;
            default:
                assert false;
                return null;
        }

        return new TabSelectionEditorActionProvider(mTabSelectionEditorController, action) {
            @Override
            void processSelectedTabs(List<Tab> selectedTabs, TabModelSelector tabModelSelector) {
                int totalTabCountBeforeProcess = tabModelSelector.getCurrentModel().getCount();
                List<Integer> selectedTabIds = new ArrayList<>();
                for (int i = 0; i < selectedTabs.size(); i++) {
                    selectedTabIds.add(selectedTabs.get(i).getId());
                }
                accepted(selectedTabIds, totalTabCountBeforeProcess);

                super.processSelectedTabs(selectedTabs, tabModelSelector);
            }
        };
    }

    @VisibleForTesting
    TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider getNavigationProvider() {
        return new TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider(
                mTabSelectionEditorController) {
            @Override
            public void goBack() {
                super.goBack();

                assert mCurrentTabSuggestionFeedback != null;
                mCurrentTabSuggestionFeedback.onResult(
                        new TabSuggestionFeedback(mCurrentBestTabSuggestion, DISMISSED, null, 0));
            }
        };
    }

    private List<Tab> getTabList() {
        List<Tab> tabs = new ArrayList<>();

        Set<Integer> suggestedTabIds = new HashSet<>();
        List<TabContext.TabInfo> suggestedTabInfo = mCurrentBestTabSuggestion.getTabsInfo();
        for (int i = 0; i < suggestedTabInfo.size(); i++) {
            suggestedTabIds.add(suggestedTabInfo.get(i).id);
            tabs.add(mTabModelSelector.getTabById(suggestedTabInfo.get(i).id));
        }

        tabs.addAll(getNonSuggestedTabs(suggestedTabIds));
        return tabs;
    }

    private List<Tab> getNonSuggestedTabs(Set<Integer> suggestedTabIds) {
        List<Tab> tabs = new ArrayList<>();
        TabModelFilter tabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        List<Tab> filteredTab = tabModelFilter.getTabsWithNoOtherRelatedTabs();

        for (int i = 0; i < filteredTab.size(); i++) {
            Tab tab = filteredTab.get(i);
            if (!suggestedTabIds.contains(tab.getId())) tabs.add(tab);
        }
        return tabs;
    }

    @VisibleForTesting
    public void dismiss() {
        assert mCurrentTabSuggestionFeedback != null;
        assert mCurrentBestTabSuggestion != null;
        mCurrentTabSuggestionFeedback.onResult(
                new TabSuggestionFeedback(mCurrentBestTabSuggestion, NOT_CONSIDERED, null, 0));
    }

    private void accepted(List<Integer> selectedTabIds, int totalTabCount) {
        assert mCurrentTabSuggestionFeedback != null;
        assert mCurrentBestTabSuggestion != null;
        mCurrentTabSuggestionFeedback.onResult(new TabSuggestionFeedback(
                mCurrentBestTabSuggestion, ACCEPTED, selectedTabIds, totalTabCount));
    }

    // TabSuggestionObserver implementations.
    @Override
    public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
            Callback<TabSuggestionFeedback> tabSuggestionFeedback) {
        if (tabSuggestions.size() == 0) return;

        sSuggestionAvailableForTesting = true;
        mCurrentBestTabSuggestion = tabSuggestions.get(0);
        mCurrentTabSuggestionFeedback = tabSuggestionFeedback;
        sendAvailabilityNotification(new TabSuggestionMessageData(
                mCurrentBestTabSuggestion, this::review, (int messageType) -> dismiss()));
    }

    @Override
    public void onTabSuggestionInvalidated() {
        mCurrentBestTabSuggestion = null;
        mCurrentTabSuggestionFeedback = null;
        sSuggestionAvailableForTesting = false;
        sendInvalidNotification();
    }

    @VisibleForTesting
    public static boolean isSuggestionAvailableForTesting() {
        return sSuggestionAvailableForTesting;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabListMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class TabListMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String NEW_TITLE = "New title";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabListFaviconProvider mTabListFaviconProvider;
    @Mock
    RecyclerView mRecyclerView;
    @Mock
    RecyclerView.Adapter mAdapter;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    EmptyTabModelFilter mEmptyTabModelFilter;
    @Mock
    TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock
    TabListMediator.CreateGroupButtonProvider mCreateGroupButtonProvider;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconCallbackCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private TabListMediator mMediator;
    private TabListModel mModel;
    private TabGridViewHolder mViewHolder1;
    private TabGridViewHolder mViewHolder2;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mViewHolder1 = prepareViewHolder(TAB1_ID, POSITION1);
        mViewHolder2 = prepareViewHolder(TAB2_ID, POSITION2);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(any(), any(), anyBoolean(), anyBoolean());
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForUrlAsync(anyString(), anyBoolean(), mCallbackCaptor.capture());
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);

        mModel = new TabListModel();
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                false, null, null, null, null, getClass().getSimpleName());
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void updatesTitle() {
        initAndAssertAllProperties();

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        doReturn(NEW_TITLE).when(mTab1).getTitle();
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    public void updatesFavicon() {
        initAndAssertAllProperties();

        assertNotNull(mModel.get(0).get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, null);

        assertNull(mModel.get(0).get(TabProperties.FAVICON));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .get(TabProperties.TAB_SELECTED_LISTENER)
                .run(mModel.get(1).get(TabProperties.TAB_ID));

        verify(mTabModel).setIndex(eq(1), anyInt());
    }

    @Test
    public void sendsCloseSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .get(TabProperties.TAB_CLOSED_LISTENER)
                .run(mModel.get(1).get(TabProperties.TAB_ID));

        verify(mTabModel).closeTab(eq(mTab2), eq(null), eq(false), eq(false), eq(true));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMoveTabSignalCorrectlyWithoutGroup() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(true);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMoveTabSignalCorrectlyWithinGroup() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        mMediator.getItemTouchHelperCallback(0f, 0f, 0f)
                .onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMergeTabSignalCorrectly() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mMediator.setActionOnAllRelatedTabsForTest(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(true);
        itemTouchHelperCallback.setHoveredTabIndexForTest(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTest(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mViewHolder1);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(mViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
    }

    @Test
    @Features.DisableFeatures({TAB_GROUPS_ANDROID, TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void neverSendsMergeTabSignal_Without_Group() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
        mMediator.setActionOnAllRelatedTabsForTest(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(true);
        itemTouchHelperCallback.setHoveredTabIndexForTest(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTest(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mViewHolder1);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(mViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void neverSendsMergeTabSignal_With_Group_Without_Group_Improvement() {
        initAndAssertAllProperties();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mMediator.setActionOnAllRelatedTabsForTest(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(true);
        itemTouchHelperCallback.setHoveredTabIndexForTest(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTest(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mViewHolder1);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(mViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void
    sendsUngroupSignalCorrectly() {
        initAndAssertAllProperties();
        setUpForTabGroupOperation();
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(false);
        itemTouchHelperCallback.setUnGroupTabIndexForTest(POSITION1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mViewHolder1);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the ungroup action.
        itemTouchHelperCallback.onSelectedChanged(mViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(eq(TAB1_ID));
    }

    @Test
    public void tabClosure() {
        initAndAssertAllProperties();

        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().willCloseTab(prepareTab(TAB3_ID, TAB3_TITLE), false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTest(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_GTS_Skip() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTest(true);

        // Add a new tab to the group with mTab2.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS_Middle() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTest(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(newTab).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAdditionEnd() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAdditionMiddle() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabSelection() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabClosureUndone() {
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(newTab);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabMergeIntoGroup() {
        setUpForTabGroupOperation();
        // Setup the mediator with a CreateGroupButtonProvider.
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                true, mCreateGroupButtonProvider, null, null, null, getClass().getSimpleName());

        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        // Assume that reset in TabGroupModelFilter is finished.
        doReturn(new ArrayList<>(Arrays.asList(mTab1, mTab2)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(POSITION2));

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_Moved_Tab_Selected() {
        setUpForTabGroupOperation();
        // Setup the mediator with a CreateGroupButtonProvider.
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                true, mCreateGroupButtonProvider, null, null, null, getClass().getSimpleName());

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2));
        mMediator.resetWithListOfTabs(tabs, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION2);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_Origin_Tab_Selected() {
        setUpForTabGroupOperation();
        // Setup the mediator with a CreateGroupButtonProvider.
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                true, mCreateGroupButtonProvider, null, null, null, getClass().getSimpleName());

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void tabMoveOutOfGroup_Dialog() {
        setUpForTabGroupOperation();

        // Setup the mediator with a DialogHandler.
        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                true, null, null, null, mTabGridDialogHandler, getClass().getSimpleName());

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithoutGroup_Forward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithoutGroup_Backward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Forward() {
        setUpForTabGroupOperation();

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Backward() {
        setUpForTabGroupOperation();

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_Forward() {
        setUpForTabGroupOperation();

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(
                mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_Backward() {
        setUpForTabGroupOperation();

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(
                mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    private void initAndAssertAllProperties() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        mMediator.resetWithListOfTabs(tabs, false);
        for (Callback<Drawable> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(new ColorDrawable(Color.RED));
        }

        assertThat(mModel.size(), equalTo(2));

        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        assertThat(mModel.get(0).get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        assertThat(mModel.get(0).get(TabProperties.FAVICON), instanceOf(Drawable.class));
        assertThat(mModel.get(1).get(TabProperties.FAVICON), instanceOf(Drawable.class));

        assertThat(mModel.get(0).get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).get(TabProperties.IS_SELECTED), equalTo(false));

        assertThat(mModel.get(0).get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));
        assertThat(mModel.get(1).get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));

        assertThat(mModel.get(0).get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));

        assertThat(mModel.get(0).get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(true).when(tab).isIncognito();
        return tab;
    }

    private TabGridViewHolder prepareViewHolder(int id, int position) {
        TabGridViewHolder viewHolder = mock(TabGridViewHolder.class);
        doReturn(id).when(viewHolder).getTabId();
        doReturn(position).when(viewHolder).getAdapterPosition();
        return viewHolder;
    }

    private TabGridItemTouchHelperCallback getItemTouchHelperCallback() {
        return (TabGridItemTouchHelperCallback) mMediator.getItemTouchHelperCallback(0f, 0f, 0f);
    }

    private void setUpForTabGroupOperation() {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        mMediator = new TabListMediator(mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, null, mTabListFaviconProvider,
                true, null, null, null, null, getClass().getSimpleName());

        initAndAssertAllProperties();
    }
}

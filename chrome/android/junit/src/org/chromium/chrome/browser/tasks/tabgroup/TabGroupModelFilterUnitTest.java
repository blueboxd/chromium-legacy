// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tabgroup;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupModelFilter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupModelFilterUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 147;
    private static final int TAB5_ID = 258;
    private static final int TAB6_ID = 369;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;
    private static final int TAB4_ROOT_ID = TAB4_ID;
    private static final int TAB5_ROOT_ID = TAB5_ID;
    private static final int TAB6_ROOT_ID = TAB5_ID;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int POSITION4 = 3;
    private static final int POSITION5 = 4;
    private static final int POSITION6 = 5;

    private static final int NEW_TAB_ID = 159;

    @Mock
    TabModel mTabModel;

    @Mock
    TabGroupModelFilter.Observer mTabGroupModelFilterObserver;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private Tab mTab5;
    private Tab mTab6;
    private List<Tab> mTabs = new ArrayList<>();

    private TabGroupModelFilter mTabGroupModelFilter;

    private Tab prepareTab(int tabId, int rootId) {
        Tab tab = mock(Tab.class);

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                int newRootId = invocation.getArgument(0);
                doReturn(newRootId).when(tab).getRootId();
                return null;
            }
        }).when(tab).setRootId(anyInt());

        doReturn(tabId).when(tab).getId();
        tab.setRootId(rootId);

        return tab;
    }

    private void setRootId(Tab tab, int rootId) {
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                int newRootId = invocation.getArgument(0);
                doReturn(newRootId).when(tab).getRootId();
                return null;
            }
        }).when(tab).setRootId(rootId);
    }

    private void setUpTab() {
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID);
        mTab4 = prepareTab(TAB4_ID, TAB4_ROOT_ID);
        mTab5 = prepareTab(TAB5_ID, TAB5_ROOT_ID);
        mTab6 = prepareTab(TAB6_ID, TAB6_ROOT_ID);
    }

    private void setUpTabModel() {
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                Tab tab = invocation.getArgument(0);
                mTabs.add(tab);
                return null;
            }
        }).when(mTabModel).addTab(any(Tab.class), anyInt(), anyInt());

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                int movedTabId = invocation.getArgument(0);
                int newIndex = invocation.getArgument(1);

                int oldIndex = TabModelUtils.getTabIndexById(mTabModel, movedTabId);
                Tab tab = TabModelUtils.getTabById(mTabModel, movedTabId);

                mTabs.remove(tab);
                if (oldIndex < newIndex) --newIndex;
                mTabs.add(newIndex, tab);
                mTabModelObserverCaptor.getValue().didMoveTab(tab, newIndex, oldIndex);
                return null;
            }
        }).when(mTabModel).moveTab(anyInt(), anyInt());

        doAnswer(new Answer() {
            @Override
            public Tab answer(InvocationOnMock invocation) throws Throwable {
                int index = invocation.getArgument(0);
                return mTabs.get(index);
            }
        }).when(mTabModel).getTabAt(anyInt());

        doAnswer(new Answer() {
            @Override
            public Integer answer(InvocationOnMock invocation) throws Throwable {
                Tab tab = invocation.getArgument(0);
                return mTabs.indexOf(tab);
            }
        }).when(mTabModel).indexOf(any(Tab.class));

        doAnswer(new Answer() {
            @Override
            public Integer answer(InvocationOnMock invocation) throws Throwable {
                return mTabs.size();
            }
        }).when(mTabModel).getCount();

        doReturn(0).when(mTabModel).index();
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
    }

    private Tab addTabToTabModel() {
        Tab tab = prepareTab(NEW_TAB_ID, NEW_TAB_ID);
        mTabModel.addTab(tab, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(tab, TabLaunchType.FROM_CHROME_UI);
        return tab;
    }

    private void setupTabGroupModelFilter(boolean isTabRestoreCompleted) {
        mTabGroupModelFilter = new TabGroupModelFilter(mTabModel);
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        mTabModel.addTab(mTab1, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab1, TabLaunchType.FROM_CHROME_UI);

        mTabModel.addTab(mTab2, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab2, TabLaunchType.FROM_CHROME_UI);

        mTabModel.addTab(mTab3, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab3, TabLaunchType.FROM_CHROME_UI);

        mTabModel.addTab(mTab4, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab4, TabLaunchType.FROM_CHROME_UI);

        mTabModel.addTab(mTab5, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab5, TabLaunchType.FROM_CHROME_UI);

        mTabModel.addTab(mTab6, -1, TabLaunchType.FROM_CHROME_UI);
        mTabModelObserverCaptor.getValue().didAddTab(mTab6, TabLaunchType.FROM_CHROME_UI);

        if (isTabRestoreCompleted) {
            mTabGroupModelFilter.restoreCompleted();
        }
    }

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        setUpTab();
        setUpTabModel();
        setupTabGroupModelFilter(true);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void moveTabOutOfGroup_No_Update_TabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB6_ID);

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab6.getRootId(), equalTo(TAB6_ID));
    }

    @Test
    public void moveTabOutOfGroup_NonRoot_Tab() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab4, mTab5, mTab6, mTab3));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB3_ID);

        verify(mTabModel).moveTab(mTab3.getId(), mTabs.size());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveTabOutOfGroup_Root_Tab_First_Tab() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab4, mTab5, mTab6, mTab2));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);

        verify(mTabModel).moveTab(mTab2.getId(), mTabs.size());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveTabOutOfGroup_Root_Tab_Not_First_Tab() {
        // Move the root tab so that first tab in group is not the root tab.
        mTabModel.moveTab(TAB2_ID, POSITION4);
        List<Tab> tabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertArrayEquals(mTabs.toArray(), tabModelBeforeUngroup.toArray());

        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab4, mTab5, mTab6, mTab2));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);

        verify(mTabModel).moveTab(mTab2.getId(), mTabs.size());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeTabToGroup_No_Update_TabModel() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4));

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab2.getId());

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeTabToGroup_Update_TabModel() {
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabModel).moveTab(mTab5.getId(), POSITION3 + 1);
    }

    @Test
    public void mergeOneTabToTab_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab4));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION1;

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab1.getId());

        verify(mTabModel).moveTab(mTab4.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToTab_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6, mTab2, mTab3, mTab4));
        int startIndex = POSITION1;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab1.getId());

        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab1.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToGroup_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());

        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab2.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeOneTabToTab_Backward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab1));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId());

        verify(mTabModel).moveTab(mTab1.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab1.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToTab_Backward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab2, mTab3));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab4.getId());

        verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab3, mTab4.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_All_Backward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6, mTab1, mTab4));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, mTab4));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab5);

        verify(mTabModel).moveTab(mTab1.getId(), POSITION6 + 1);
        verify(mTabModel).moveTab(mTab4.getId(), POSITION6 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab5.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab5.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_All_Forward() {
        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab4, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, newTab, mTab2, mTab3, mTab5, mTab6));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab1);

        verify(mTabModel).moveTab(mTab4.getId(), POSITION1 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION1 + 2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab1.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_Any_direction() {
        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, newTab, mTab5, mTab6));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4);

        verify(mTabModel).moveTab(mTab1.getId(), POSITION4 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab4.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveGroup_Backward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.moveRelatedTabs(mTab2.getId(), startIndex + 1);

        verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabGroup(mTab3, POSITION3 - 1, startIndex);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveGroup_Forward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.moveRelatedTabs(mTab5.getId(), startIndex + 1);

        verify(mTabModel).moveTab(mTab5.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab6.getId(), startIndex + 2);
        verify(mTabGroupModelFilterObserver).didMoveTabGroup(mTab6, POSITION6, startIndex + 2);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void ignoreUnrelatedMoveTab() {
        // Simulate that the tab restoring is not yet finished.
        setupTabGroupModelFilter(false);

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION1, POSITION6);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION6, POSITION1);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION5);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION5, POSITION2);

        // No call should be made here.
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabOutOfGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveWithinGroup(any(Tab.class), anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabGroup(any(Tab.class), anyInt(), anyInt());
    }
}

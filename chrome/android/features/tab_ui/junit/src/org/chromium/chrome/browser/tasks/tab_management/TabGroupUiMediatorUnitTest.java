// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupUiMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUiMediatorUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;

    @Mock
    BottomControlsCoordinator.BottomControlsVisibilityController mVisibilityController;
    @Mock
    TabGroupUiMediator.ResetHandler mResetHandler;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreatorManager.TabCreator mTabCreator;
    @Mock
    OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    ThemeColorProvider mThemeColorProvider;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<OverviewModeBehavior.OverviewModeObserver> mOverviewModeObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> mThemeColorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.TintObserver> mTintObserverArgumentCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private PropertyModel mModel;
    private TabGroupUiMediator mTabGroupUiMediator;
    private boolean mIsVisible;
    private InOrder mVisibilityControllerInOrder;

    private Tab prepareTab(int tabId, int rootId) {
        Tab tab = mock(Tab.class);
        doReturn(tabId).when(tab).getId();
        doReturn(rootId).when(tab).getRootId();
        return tab;
    }

    private void initAndAssertProperties(boolean tabModelHasTab) {
        if (!tabModelHasTab) {
            doReturn(TabModel.INVALID_TAB_INDEX).when(mTabModel).index();
            doReturn(0).when(mTabModel).getCount();
            doReturn(0).when(mTabGroupModelFilter).getCount();
            doReturn(null).when(mTabModelSelector).getCurrentTab();
        }

        mTabGroupUiMediator = new TabGroupUiMediator(mVisibilityController, mResetHandler, mModel,
                mTabModelSelector, mTabCreatorManager, mOverviewModeBehavior, mThemeColorProvider);

        if (!tabModelHasTab) {
            verify(mResetHandler, never()).resetStripWithListOfTabs(any());
            mVisibilityControllerInOrder.verify(mVisibilityController, never())
                    .setBottomControlsVisible(anyBoolean());
        } else {
            verify(mResetHandler).resetStripWithListOfTabs(any());
            mVisibilityControllerInOrder.verify(mVisibilityController)
                    .setBottomControlsVisible(anyBoolean());
        }
    }

    @Before
    public void setUp() {
        // Each test must call initAndAssertProperties. After setUp() and
        // initAndAssertProperties(true), TabModel has 3 tabs in the following order: mTab1, mTab2,
        // and mTab3, while mTab2 and mTab3 are in a group. By default mTab1 is selected. If
        // initAndAssertProperties(false) is called instead, there's no tabs in TabModel.
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mIsVisible = false;

        // Set up Tabs
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID);

        // Set up TabModel.
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(3).when(mTabModel).getCount();
        doReturn(0).when(mTabModel).index();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab3).when(mTabModel).getTabAt(2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);

        // Set up TabGroupModelFilter.
        doReturn(false).when(mTabGroupModelFilter).isIncognito();
        doReturn(2).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab3);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2, mTab3));
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        // Set up TabModelSelector, TabModelFilterProvider, TabGroupModelFilter.
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doNothing().when(mTabModelSelector)
                    .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing().when(mTabModelFilterProvider)
                    .addTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        // Set up OverviewModeBehavior
        doNothing().when(mOverviewModeBehavior)
                    .addOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());

        // Set up ThemeColorProvider
        doNothing().when(mThemeColorProvider)
                    .addThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        doNothing().when(mThemeColorProvider)
                    .addTintObserver(mTintObserverArgumentCaptor.capture());

        // Set up ResetHandler
        doNothing().when(mResetHandler).resetStripWithListOfTabs(any());
        doNothing().when(mResetHandler).resetGridWithListOfTabs(any());

        // Set up TabCreatorManager
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(null).when(mTabCreator).createNewTab(any(), anyInt(), any());

        mVisibilityControllerInOrder = inOrder(mVisibilityController);
        mModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void restoreCompleted_TabModelNoTab() {
        // Simulate no tab in current TabModel.
        initAndAssertProperties(false);

        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    public void restoreCompleted_UiNotVisible() {
        // Assume mTab1 is selected, and it does not have related tabs.
        initAndAssertProperties(true);
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);
    }

    @Test
    public void restoreCompleted_UiVisible() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(true);
        doReturn(POSITION2).when(mTabModel).index();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(true);
    }
}
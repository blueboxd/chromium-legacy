// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGridDialogMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGridDialogMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String DIALOG_TITLE1 = "1 Tab";
    private static final String DIALOG_TITLE2 = "2 Tabs";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    Rect mRect;
    @Mock
    View mView;
    @Mock
    TabGridDialogMediator.ResetHandler mDialogResetHandler;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreatorManager.TabCreator mTabCreator;
    @Mock
    TabSwitcherMediator.ResetHandler mTabSwitcherResetHandler;
    @Mock
    TabGridDialogMediator.AnimationOriginProvider mAnimationOriginProvider;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabModel mTabModel;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private PropertyModel mModel;
    private TabGridDialogMediator mMediator;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mResources).when(mContext).getResources();
        doReturn(DIALOG_TITLE1)
                .when(mResources)
                .getQuantityString(
                        org.chromium.chrome.R.plurals.bottom_tab_grid_title_placeholder, 1, 1);
        doReturn(DIALOG_TITLE2)
                .when(mResources)
                .getQuantityString(
                        org.chromium.chrome.R.plurals.bottom_tab_grid_title_placeholder, 2, 2);
        doReturn(mRect).when(mAnimationOriginProvider).getAnimationOriginRect(anyInt());
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());

        mModel = new PropertyModel(TabGridSheetProperties.ALL_KEYS);
        mMediator =
                new TabGridDialogMediator(mContext, mDialogResetHandler, mModel, mTabModelSelector,
                        mTabCreatorManager, mTabSwitcherResetHandler, mAnimationOriginProvider);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void setupListenersAndObservers() {
        // These listeners and observers should be setup when the mediator is created.
        assertThat(mModel.get(TabGridSheetProperties.SCRIMVIEW_OBSERVER),
                instanceOf(ScrimView.ScrimObserver.class));
        assertThat(mModel.get(TabGridSheetProperties.COLLAPSE_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
        assertThat(mModel.get(TabGridSheetProperties.ADD_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
    }

    @Test
    public void onClickAdd_HasCurrentTab() {
        // Mock that the animation source Rect is not null.
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, mRect);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        View.OnClickListener listener = mModel.get(TabGridSheetProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(null));
        verify(mDialogResetHandler).resetWithListOfTabs(null);
        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_CHROME_UI), eq(mTab1));
    }

    @Test
    public void onClickAdd_NoCurrentTab() {
        mMediator.setCurrentTabIdForTest(Tab.INVALID_TAB_ID);

        View.OnClickListener listener = mModel.get(TabGridSheetProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mTabCreator).launchNTP();
    }

    @Test
    public void onClickCollapse() {
        View.OnClickListener listener = mModel.get(TabGridSheetProperties.COLLAPSE_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mDialogResetHandler).resetWithListOfTabs(null);
    }

    @Test
    public void onClickScrim() {
        ScrimView.ScrimObserver observer = mModel.get(TabGridSheetProperties.SCRIMVIEW_OBSERVER);
        observer.onScrimClick();

        verify(mDialogResetHandler).resetWithListOfTabs(null);
    }

    @Test
    public void tabAddition() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source Rect is not null.
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, mRect);

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(null));
        verify(mDialogResetHandler).resetWithListOfTabs(null);
    }

    @Test
    public void tabClosure_NotLast_NotCurrent() {
        // Assume that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Assume tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        // Assume dialog title is null and the dialog is showing.
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should not update.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabClosure_NotLast_Current() {
        // Assume that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Assume tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB2_ID);
        // Assume dialog title is null and the dialog is showing.
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabClosure_Last_Current() {
        // Assume that tab1 is the last tab in the group and it just gets closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        // As last tab in the group, tab1 is definitely the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB1_ID);
        // Assume the dialog is showing and the source Rect is not null.
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, mRect);
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false);

        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(Tab.INVALID_TAB_ID));
        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(null));
        verify(mDialogResetHandler).resetWithListOfTabs(null);
        verify(mTabSwitcherResetHandler, never()).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabClosure_NotLast_Current_WithDialogHidden() {
        // Assume that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Assume tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTest(TAB2_ID);
        // Assume dialog title is null and the dialog is hidden.
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTest(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never()).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabClosureUndone() {
        // Mock that the dialog is showing.
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabClosureUndone_WithDialogHidden() {
        // Mock that the dialog is hidden.
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTest(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never()).resetWithTabList(mTabGroupModelFilter, false);
    }

    @Test
    public void tabSelection() {
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(null));
        verify(mDialogResetHandler).resetWithListOfTabs(null);
    }

    @Test
    public void hideDialog_Animation() {
        FeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(true);

        // Mock that the dialog is showing and animation source Rect is null.
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);

        mMediator.onReset(null);

        assertThat(mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE), equalTo(false));
        // Animation source Rect should be updated with specific Rect.
        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(null));

        FeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(null);
    }

    @Test
    public void hideDialog_NoAnimation() {
        FeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(false);

        // Mock that the dialog is showing and animation source Rect is null.
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);

        mMediator.onReset(null);

        assertThat(mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE), equalTo(false));
        // Animation source Rect should be updated with specific Rect.
        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(mRect));

        FeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(null);
    }

    @Test
    public void showDialog() {
        // Mock that the dialog is hidden and animation source Rect and header title are all null.
        mModel.set(TabGridSheetProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridSheetProperties.ANIMATION_SOURCE_RECT, null);
        mModel.set(TabGridSheetProperties.HEADER_TITLE, null);

        mMediator.onReset(TAB1_ID);

        assertThat(mModel.get(TabGridSheetProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source Rect should be updated with specific Rect.
        assertThat(mModel.get(TabGridSheetProperties.ANIMATION_SOURCE_RECT), equalTo(mRect));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridSheetProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
    }

    @Test
    public void destroy() {
        mMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverCaptor.capture());
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn("").when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(true).when(tab).isIncognito();
        return tab;
    }
}

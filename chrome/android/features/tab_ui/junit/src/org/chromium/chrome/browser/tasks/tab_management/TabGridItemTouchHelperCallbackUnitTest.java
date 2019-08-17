// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link TabGridItemTouchHelperCallback}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
        ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
@Config(manifest = Config.NONE)
public class TabGridItemTouchHelperCallbackUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int POSITION4 = 3;
    private static final float THRESHOLD = 2f;

    @Mock
    Canvas mCanvas;
    @Mock
    RecyclerView mRecyclerView;
    @Mock
    RecyclerView.Adapter mAdapter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabListMediator.TabActionListener mTabClosedListener;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    EmptyTabModelFilter mEmptyTabModelFilter;
    @Mock
    TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock
    Profile mProfile;
    @Mock
    Tracker mTracker;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private TabGridViewHolder mMockViewHolder1;
    private TabGridViewHolder mMockViewHolder2;
    private RecyclerView.ViewHolder mDummyViewHolder1;
    private RecyclerView.ViewHolder mDummyViewHolder2;
    private RecyclerView.ViewHolder mDummyViewHolder3;
    private RecyclerView.ViewHolder mDummyViewHolder4;
    private View mItemView1;
    private View mItemView2;
    private View mItemView3;
    private View mItemView4;
    private TabGridItemTouchHelperCallback mItemTouchHelperCallback;
    private TabListModel mModel;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = prepareTab(TAB3_ID, TAB3_TITLE);
        mTab4 = prepareTab(TAB4_ID, TAB4_TITLE);
        mMockViewHolder1 = prepareMockViewHolder(TAB1_ID, POSITION1);
        mMockViewHolder2 = prepareMockViewHolder(TAB2_ID, POSITION2);
        // Mock four cards in a grid layout. Each card is of width 4 and height 4. Both the side
        // gaps and top gaps between adjacent cards are 1.
        mItemView1 = prepareItemView(0, 0, 4, 4);
        mItemView2 = prepareItemView(5, 0, 9, 4);
        mItemView3 = prepareItemView(0, 5, 4, 9);
        mItemView4 = prepareItemView(5, 5, 9, 9);
        mDummyViewHolder1 = prepareDummyViewHolder(mItemView1);
        mDummyViewHolder2 = prepareDummyViewHolder(mItemView2);
        mDummyViewHolder3 = prepareDummyViewHolder(mItemView3);
        mDummyViewHolder4 = prepareDummyViewHolder(mItemView4);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab3).when(mTabModel).getTabAt(POSITION3);
        doReturn(mTab4).when(mTabModel).getTabAt(POSITION4);
        doReturn(4).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(mTab3).when(mTabGroupModelFilter).getTabAt(POSITION3);
        doReturn(mTab4).when(mTabGroupModelFilter).getTabAt(POSITION4);
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        setupRecyclerView();

        mModel = new TabListModel();
        setupItemTouchHelperCallback(false);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    private void setupRecyclerView() {
        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(12).when(mRecyclerView).getBottom();
        doReturn(4).when(mRecyclerView).getChildCount();
        doReturn(4).when(mAdapter).getItemCount();
        when(mRecyclerView.getChildAt(POSITION1)).thenReturn(mItemView1);
        when(mRecyclerView.getChildAt(POSITION2)).thenReturn(mItemView2);
        when(mRecyclerView.getChildAt(POSITION3)).thenReturn(mItemView3);
        when(mRecyclerView.getChildAt(POSITION4)).thenReturn(mItemView4);
        doReturn(mRecyclerView).when(mItemView1).getParent();
        doReturn(mRecyclerView).when(mItemView2).getParent();
        doReturn(mRecyclerView).when(mItemView3).getParent();
        doReturn(mRecyclerView).when(mItemView4).getParent();
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION1))
                .thenReturn(mDummyViewHolder1);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION2))
                .thenReturn(mDummyViewHolder2);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION3))
                .thenReturn(mDummyViewHolder3);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION4))
                .thenReturn(mDummyViewHolder4);
    }

    private void setupItemTouchHelperCallback(boolean isDialog) {
        mItemTouchHelperCallback = new TabGridItemTouchHelperCallback(mModel, mTabModelSelector,
                mTabClosedListener, isDialog ? mTabGridDialogHandler : null, "", !isDialog);
        mItemTouchHelperCallback.setupCallback(THRESHOLD, THRESHOLD, THRESHOLD, mProfile);
        mItemTouchHelperCallback.getMovementFlags(mRecyclerView, mMockViewHolder1);
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
    public void onStartDraggingTab() {
        initAndAssertAllProperties();

        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(1f));

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);

        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN));
        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(0.8f));
    }

    @Test
    public void onSwipeTab_DELETE() {
        initAndAssertAllProperties();

        mItemTouchHelperCallback.onSwiped(mMockViewHolder1, POSITION1);

        verify(mTabClosedListener).run(TAB1_ID);
    }

    @Test
    public void onReleaseTab_NO_MERGE() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0).set(TabProperties.CARD_ANIMATION_STATUS,
                ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).set(TabProperties.ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTest(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(1f));
        assertThat(mModel.get(1).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(1).get(TabProperties.ALPHA), equalTo(1f));
    }

    @Test
    public void onReleaseTab_MERGE_BACKWARD() {
        initAndAssertAllProperties();

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0).set(TabProperties.CARD_ANIMATION_STATUS,
                ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).set(TabProperties.ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTest(POSITION1);

        // Simulate hovering on card#2.
        mItemTouchHelperCallback.setHoveredTabIndexForTest(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mRecyclerView).removeView(mItemView1);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_OUT));
        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(1f));
    }

    @Test
    public void onReleaseTab_MERGE_FORWARD() {
        initAndAssertAllProperties();

        // Simulate the selection of card#2 in TabListModel.
        mModel.get(1).set(TabProperties.CARD_ANIMATION_STATUS,
                ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(1).set(TabProperties.ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTest(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTest(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mRecyclerView).removeView(mItemView2);
        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(mModel.get(1).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(1).get(TabProperties.ALPHA), equalTo(1f));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void onReleaseTab_MERGE_BACKWARD_WITHOUT_GROUP() {
        initAndAssertAllProperties();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0).set(TabProperties.CARD_ANIMATION_STATUS,
                ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).set(TabProperties.ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTest(POSITION1);

        // Simulate hovering on card#2.
        mItemTouchHelperCallback.setHoveredTabIndexForTest(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(1f));
        // Merge signal should never be sent.
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
        verify(mRecyclerView, never()).removeView(any(View.class));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void onReleaseTab_MERGE_FORWARD_WITHOUT_GROUP() {
        initAndAssertAllProperties();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        // Simulate the selection of card#2 in TabListModel.
        mModel.get(1).set(TabProperties.CARD_ANIMATION_STATUS,
                ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(1).set(TabProperties.ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTest(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTest(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        assertThat(mModel.get(1).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(1).get(TabProperties.ALPHA), equalTo(1f));
        // Merge signal should never be sent.
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
        verify(mRecyclerView, never()).removeView(any(View.class));
    }

    @Test
    public void onReleaseTab_UNGROUP_BAR_HIDE() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HIDE);
    }

    @Test
    public void onReleaseTab_UNGROUP() {
        initAndAssertAllProperties();

        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTest(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(TAB1_ID);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HIDE);
        verify(mRecyclerView).removeView(mItemView1);
    }

    @Test
    public void onDragTab_HOVERED_GTS_HORIZONTAL() {
        initAndAssertAllProperties();

        // Drag card#1 rightwards to hover on card#2.
        verifyDrag(mDummyViewHolder1, 5, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 leftwards to hover on card#1.
        verifyDrag(mDummyViewHolder2, -5, 0, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_HOVERED_GTS_VERTICAL() {
        initAndAssertAllProperties();

        // Drag card#1 downwards to hover on card#3.
        verifyDrag(mDummyViewHolder1, 0, 5, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 upwards to hover on card#1.
        verifyDrag(mDummyViewHolder3, 0, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_HOVERED_GTS_DIAGONAL() {
        initAndAssertAllProperties();

        // Drag card#1 diagonally to hover on card#4.
        verifyDrag(mDummyViewHolder1, 5, 5, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#4 diagonally to hover on card#1.
        verifyDrag(mDummyViewHolder4, -5, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 diagonally to hover on card#3.
        verifyDrag(mDummyViewHolder2, -5, 5, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 diagonally to hover on card#2.
        verifyDrag(mDummyViewHolder3, 5, -5, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_NOT_HOVERED_GTS_HORIZONTAL() {
        initAndAssertAllProperties();

        // With merge threshold equal to 2, any horizontal drag with |dX| <= (5 - threshold) should
        // never trigger hovering.
        verifyDrag(mDummyViewHolder1, 3, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder2, -3, 0, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 2, any horizontal drag with |dX| >= (5 + threshold) should
        // never trigger hovering.
        verifyDrag(mDummyViewHolder1, 7, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder2, -7, 0, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NOT_HOVERED_GTS_VERTICAL() {
        initAndAssertAllProperties();

        // With merge threshold equal to 2, any vertical drag with |dY| <= (5 - threshold) should
        // never trigger hovering.
        verifyDrag(mDummyViewHolder1, 0, 3, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder3, 0, -3, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 2, any vertical drag with |dY| >= (5 + threshold) should
        // never trigger hovering.
        verifyDrag(mDummyViewHolder1, 0, 7, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder3, 0, -7, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NOT_HOVERED_GTS_DIAGONAL() {
        initAndAssertAllProperties();

        // With merge threshold equal to 2, any diagonal drag with |dX| <= (5 - threshold) or |dY|
        // <= (5 - threshold) should never trigger hovering.
        verifyDrag(mDummyViewHolder1, 3, 4, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder1, 4, 3, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -4, -3, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -3, -4, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 2, any vertical drag with |dX| >= (5 + threshold) or |dY|
        // >= (5 + threshold) should never trigger hovering.
        verifyDrag(mDummyViewHolder1, 7, 6, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder1, 6, 7, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -6, -7, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -7, -6, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Restore_Hovered_GTS() {
        initAndAssertAllProperties();

        // Simulate the process of hovering card#1 on card#2.
        verifyDrag(mDummyViewHolder1, 5, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 horizontally so that it is no longer hovering on card#2.
        verifyDrag(mDummyViewHolder1, 10, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#3.
        verifyDrag(mDummyViewHolder1, 0, 5, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 vertically so that it is no longer hovering on card#3.
        verifyDrag(mDummyViewHolder1, 0, 10, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#4.
        verifyDrag(mDummyViewHolder1, 5, 5, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 diagonally so that it is no longer hovering on card#4.
        verifyDrag(mDummyViewHolder1, 10, 10, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.HOVERED_CARD_ZOOM_OUT);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void onDragTab_Hovered_GTS_WITHOUT_GROUP() {
        initAndAssertAllProperties();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        // Hovering shouldn't make any difference.
        verifyDrag(mDummyViewHolder1, 5, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder2, -5, 0, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);

        verifyDrag(mDummyViewHolder1, 0, 5, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder3, 0, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);

        verifyDrag(mDummyViewHolder1, 5, 5, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -5, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Hovered_NON_GTS() {
        initAndAssertAllProperties();
        // Suppose drag happens in components other than GTS.
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTest(false);

        // Hovering shouldn't make any difference.
        verifyDrag(mDummyViewHolder1, 5, 0, POSITION2,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder2, -5, 0, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);

        verifyDrag(mDummyViewHolder1, 0, 5, POSITION3,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder3, 0, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);

        verifyDrag(mDummyViewHolder1, 5, 5, POSITION4,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
        verifyDrag(mDummyViewHolder4, -5, -5, POSITION1,
                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_UNGROUP() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // Simulate dragging card#1 down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, mDummyViewHolder1, 0, 7,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HOVERED);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, mDummyViewHolder1, 0, 2,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_NOT_UNGROUP() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // With recyclerview bottom equal to 12 and ungroup threshold equal to 2, any drag with
        // itemview.bottom + dY <= 10 should never trigger ungroup.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, mDummyViewHolder1, 0, 6,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, mDummyViewHolder3, 0, 1,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        verify(mTabGridDialogHandler, times(2))
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HIDE);

        verify(mTabGridDialogHandler, never())
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_UNGROUP_LAST_TAB() {
        initAndAssertAllProperties();
        setupItemTouchHelperCallback(true);

        // Assume that there is only one tab in the group.
        doReturn(1).when(mAdapter).getItemCount();

        // Simulate dragging the only tab down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, mDummyViewHolder1, 0, 8,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        verify(mTabGridDialogHandler, never())
                .updateUngroupBarStatus(TabGridDialogParent.UngroupBarStatus.HOVERED);
    }

    private void verifyDrag(
            RecyclerView.ViewHolder viewHolder, float dX, float dY, int targetIndex, int status) {
        // Simulate the process of dragging one card to a position.
        mItemTouchHelperCallback.onChildDraw(mCanvas, mRecyclerView, viewHolder, dX, dY,
                ItemTouchHelper.ACTION_STATE_DRAG, true);

        // Verify the card in target index is in correct status.
        assertThat(
                mModel.get(targetIndex).get(TabProperties.CARD_ANIMATION_STATUS), equalTo(status));
    }

    private void initAndAssertAllProperties() {
        for (int i = 0; i < mTabModel.getCount(); i++) {
            Tab tab = mTabModel.getTabAt(i);
            addTabInfoModel(tab);
        }

        assertThat(mModel.size(), equalTo(4));

        assertThat(mModel.get(0).get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(2).get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(3).get(TabProperties.TAB_ID), equalTo(TAB4_ID));

        assertThat(mModel.get(0).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(1).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(2).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(3).get(TabProperties.CARD_ANIMATION_STATUS),
                equalTo(ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE));

        assertThat(mModel.get(0).get(TabProperties.ALPHA), equalTo(1f));
        assertThat(mModel.get(1).get(TabProperties.ALPHA), equalTo(1f));
        assertThat(mModel.get(2).get(TabProperties.ALPHA), equalTo(1f));
        assertThat(mModel.get(3).get(TabProperties.ALPHA), equalTo(1f));
    }

    private void addTabInfoModel(Tab tab) {
        PropertyKey[] testKeysTabGrid = new PropertyKey[] {
                TabProperties.TAB_ID, TabProperties.CARD_ANIMATION_STATUS, TabProperties.ALPHA};
        PropertyModel tabInfo =
                new PropertyModel.Builder(testKeysTabGrid)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.CARD_ANIMATION_STATUS,
                                ClosableTabGridViewHolder.AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.ALPHA, 1f)
                        .build();
        mModel.add(tabInfo);
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(title).when(tab).getTitle();
        return tab;
    }

    private TabGridViewHolder prepareMockViewHolder(int id, int position) {
        TabGridViewHolder viewHolder = mock(TabGridViewHolder.class);
        doReturn(id).when(viewHolder).getTabId();
        doReturn(position).when(viewHolder).getAdapterPosition();
        return viewHolder;
    }

    private View prepareItemView(int left, int top, int right, int bottom) {
        View view = mock(View.class);
        doReturn(left).when(view).getLeft();
        doReturn(top).when(view).getTop();
        doReturn(right).when(view).getRight();
        doReturn(bottom).when(view).getBottom();
        return view;
    }

    private RecyclerView.ViewHolder prepareDummyViewHolder(View itemView) {
        return new RecyclerView.ViewHolder(itemView) {};
    }
}

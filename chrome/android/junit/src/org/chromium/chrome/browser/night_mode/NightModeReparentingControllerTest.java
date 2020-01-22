// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link NightModeReparentingControllerTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NightModeReparentingControllerTest {
    class FakeNightModeReparentingDelegate implements NightModeReparentingController.Delegate {
        ActivityTabProvider mActivityTabProvider;
        TabModelSelector mTabModelSelector;

        @Override
        public ActivityTabProvider getActivityTabProvider() {
            if (mActivityTabProvider == null) {
                // setup
                mActivityTabProvider = Mockito.mock(ActivityTabProvider.class);
                doAnswer(invocation -> getNextTabFromList()).when(mActivityTabProvider).get();
            }
            return mActivityTabProvider;
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            if (mTabModelSelector == null) {
                // setup
                mTabModelSelector = Mockito.mock(TabModelSelector.class);
                doAnswer(invocation -> getCurrentTabModel())
                        .when(mTabModelSelector)
                        .getCurrentModel();
                doAnswer(invocation -> getCurrentTabModel())
                        .when(mTabModelSelector)
                        .getModel(anyBoolean());
            }

            return mTabModelSelector;
        }
    }

    @Mock
    TabModel mTabModel;
    @Mock
    ReparentingTask mTask;
    @Mock
    ReparentingTask.Delegate mReparentingTaskDelegate;
    @Captor
    ArgumentCaptor<Tab> mTabCaptor;

    List<Tab> mTabs = new ArrayList<>();
    Map<Tab, Integer> mTabIndexMapping = new HashMap<>();

    NightModeReparentingController mController;
    NightModeReparentingController.Delegate mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doAnswer(invocation -> {
            removeTab(mTabCaptor.getValue());
            return null;
        })
                .when(mTabModel)
                .removeTab(mTabCaptor.capture());

        doAnswer(invocation -> getTabIndex(mTabCaptor.getValue()))
                .when(mTabModel)
                .indexOf(mTabCaptor.capture());

        mDelegate = new FakeNightModeReparentingDelegate();
        mController = new NightModeReparentingController(mDelegate, mReparentingTaskDelegate);
    }

    @After
    public void tearDown() {
        AsyncTabParamsManager.getAsyncTabParams().clear();
        mTabs.clear();
        mTabIndexMapping.clear();
    }

    @Test
    public void testReparenting_singleTab() {
        createAndAddMockTab(1, 0);
        mController.onNightModeStateChanged();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Assert.assertEquals(
                "The index of the first tab stored should match it's index in the tab stack.", 0,
                trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(1, tab.getId());
        verify(mTask, times(1)).detach();

        mController.onStartWithNative();
        verify(mTask, times(1)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_singleTab_currentModelNullOnStart() {
        createAndAddMockTab(1, 0);
        mController.onNightModeStateChanged();

        doReturn(null).when(mDelegate.getTabModelSelector()).getModel(anyBoolean());
        mController.onStartWithNative();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNull(params);
    }

    @Test
    public void testReparenting_multipleTabs_onlyOneIsReparented() {
        createAndAddMockTab(1, 0);
        createAndAddMockTab(2, 1);
        mController.onNightModeStateChanged();

        TabReparentingParams trp =
                (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertEquals(
                "The index of the first tab stored should match its index in the tab stack.", 0,
                trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(1, tab.getId());

        trp = (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertFalse("The index of the background tabs stored shouldn't have a tab index.",
                trp.hasTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());

        tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(2, tab.getId());

        verify(mTask, times(2)).detach();

        mController.onStartWithNative();
        verify(mTask, times(2)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_twoTabsOutOfOrder() {
        createAndAddMockTab(1, 1);
        createAndAddMockTab(2, 0);
        mController.onNightModeStateChanged();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Assert.assertEquals(
                "The index of the first tab stored should match its index in the tab stack.", 1,
                trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(1, tab.getId());

        verify(mTask, times(2)).detach();

        mController.onStartWithNative();
        verify(mTask, times(2)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_threeTabsOutOfOrder() {
        createAndAddMockTab(1, 1);
        createAndAddMockTab(2, 0);
        createAndAddMockTab(3, 2);
        mController.onNightModeStateChanged();

        // Check the foreground tab.
        TabReparentingParams trp =
                (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertEquals(
                "The index of the first tab stored should match its index in the tab stack.", 1,
                trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(1, tab.getId());

        // Check the background tabs.
        trp = (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertFalse("The index of the background tabs stored shouldn't have a tab index.",
                trp.hasTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());
        trp = (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(3);
        Assert.assertFalse("The index of the background tabs stored shouldn't have a tab index.",
                trp.hasTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());

        verify(mTask, times(3)).detach();

        mController.onStartWithNative();
        verify(mTask, times(3)).finish(anyObject(), anyObject());
    }

    @Test
    public void testTabGetsStored_noTab() {
        try {
            mController.onNightModeStateChanged();
            mController.onStartWithNative();
            verify(mTask, times(0)).finish(anyObject(), anyObject());
        } catch (Exception e) {
            Assert.assertTrue(
                    "NightModeReparentingController shouldn't crash even when there's no tab!",
                    false);
        }
    }

    private void createAndAddMockTab(int id, int index) {
        Tab tab = Mockito.mock(Tab.class);
        UserDataHost udh = new UserDataHost();
        udh.setUserData(ReparentingTask.class, mTask);
        doReturn(udh).when(tab).getUserDataHost();
        doReturn(id).when(tab).getId();
        mTabs.add(tab);
        mTabIndexMapping.put(tab, index);
    }

    private Tab getNextTabFromList() {
        if (mTabs.size() == 0) return null;
        return mTabs.get(0);
    }

    private void removeTab(Tab tab) {
        mTabs.remove(tab);
    }

    private TabModel getCurrentTabModel() {
        return mTabModel;
    }

    private int getTabIndex(Tab tab) {
        return mTabIndexMapping.get(tab);
    }
}

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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.HashMap;
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
        boolean mIsNTPUrl;

        @Override
        public ActivityTabProvider getActivityTabProvider() {
            if (mActivityTabProvider == null) {
                // setup
                mActivityTabProvider = Mockito.mock(ActivityTabProvider.class);
                doAnswer(invocation -> getForegroundTab()).when(mActivityTabProvider).get();
            }
            return mActivityTabProvider;
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            if (mTabModelSelector == null) {
                // setup
                mTabModelSelector = Mockito.mock(TabModelSelector.class);

                doReturn(mTabModel).when(mTabModelSelector).getModel(false);
                doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
            }

            return mTabModelSelector;
        }

        @Override
        public boolean isNTPUrl(String url) {
            return mIsNTPUrl;
        }

        public void setIsNTPUrl(boolean isNTPUrl) {
            mIsNTPUrl = isNTPUrl;
        }
    }

    @Mock
    ReparentingTask mTask;
    @Mock
    ReparentingTask.Delegate mReparentingTaskDelegate;

    MockTabModel mTabModel;
    MockTabModel mIncognitoTabModel;
    Map<Tab, Integer> mTabIndexMapping = new HashMap<>();
    Tab mForegroundTab;

    NightModeReparentingController mController;
    FakeNightModeReparentingDelegate mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTabModel = new MockTabModel(false, null);
        mIncognitoTabModel = new MockTabModel(true, null);

        mDelegate = new FakeNightModeReparentingDelegate();
        mDelegate.setIsNTPUrl(false);
        mController = new NightModeReparentingController(mDelegate, mReparentingTaskDelegate);
    }

    @After
    public void tearDown() {
        mForegroundTab = null;
        AsyncTabParamsManager.getAsyncTabParams().clear();
        mTabIndexMapping.clear();
    }

    @Test
    public void testReparenting_singleTab() {
        mForegroundTab = createAndAddMockTab(1, false);
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

        mController.onNativeInitialized();
        verify(mTask, times(1)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_singleTab_NTP() {
        mDelegate.setIsNTPUrl(true);
        mForegroundTab = createAndAddMockTab(1, false, UrlConstants.NTP_URL);
        mController.onNightModeStateChanged();

        Assert.assertFalse(AsyncTabParamsManager.hasParamsWithTabToReparent());
    }

    @Test
    public void testReparenting_singleTab_currentModelNullOnStart() {
        mForegroundTab = createAndAddMockTab(1, false);
        mController.onNightModeStateChanged();

        doReturn(null).when(mDelegate.getTabModelSelector()).getModel(anyBoolean());
        mController.onNativeInitialized();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertNull(params);
    }

    @Test
    public void testReparenting_multipleTabs() {
        mForegroundTab = createAndAddMockTab(1, false);
        createAndAddMockTab(2, false);
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
        Assert.assertEquals(1, trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());

        tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(2, tab.getId());

        verify(mTask, times(2)).detach();

        mController.onNativeInitialized();
        verify(mTask, times(2)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_twoTabsOutOfOrder() {
        createAndAddMockTab(1, false);
        mForegroundTab = createAndAddMockTab(2, false);
        mController.onNightModeStateChanged();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Assert.assertEquals(1, trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(2, tab.getId());

        verify(mTask, times(2)).detach();

        mController.onNativeInitialized();
        verify(mTask, times(2)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_twoTabsOneIncognito() {
        createAndAddMockTab(1, false);
        mForegroundTab = createAndAddMockTab(2, true);
        mController.onNightModeStateChanged();

        AsyncTabParams params = AsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertNotNull(params);
        Assert.assertTrue(params instanceof TabReparentingParams);

        TabReparentingParams trp = (TabReparentingParams) params;
        Assert.assertEquals(0, trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(2, tab.getId());

        verify(mTask, times(2)).detach();

        mController.onNativeInitialized();
        verify(mTask, times(2)).finish(anyObject(), anyObject());
    }

    @Test
    public void testReparenting_threeTabsOutOfOrder() {
        createAndAddMockTab(3, false);
        mForegroundTab = createAndAddMockTab(2, false);
        createAndAddMockTab(1, false);
        mController.onNightModeStateChanged();

        // Check the foreground tab.
        TabReparentingParams trp =
                (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(2);
        Assert.assertEquals(1, trp.getTabIndex());
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertTrue(trp.isForegroundTab());

        Tab tab = trp.getTabToReparent();
        Assert.assertNotNull(tab);
        Assert.assertEquals(2, tab.getId());

        // Check the background tabs.
        trp = (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(1);
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());
        trp = (TabReparentingParams) AsyncTabParamsManager.getAsyncTabParams().get(3);
        Assert.assertTrue(trp.isFromNightModeReparenting());
        Assert.assertFalse(trp.isForegroundTab());

        verify(mTask, times(3)).detach();

        mController.onNativeInitialized();
        verify(mTask, times(3)).finish(anyObject(), anyObject());
    }

    @Test
    public void testTabGetsStored_noTab() {
        try {
            mController.onNightModeStateChanged();
            mController.onNativeInitialized();
            verify(mTask, times(0)).finish(anyObject(), anyObject());
        } catch (Exception e) {
            Assert.assertTrue(
                    "NightModeReparentingController shouldn't crash even when there's no tab!",
                    false);
        }
    }

    /**
     * Adds a tab to the correct model and sets the index in the mapping.
     *
     * @param id The id to give to the tab.
     * @param incognito Whether to add the tab to the incognito model or the regular model.
     * @param url The url to set for the tab.
     * @return The tab that was added. Use the return value to set the foreground tab for tests.
     */
    private Tab createAndAddMockTab(int id, boolean incognito, String url) {
        Tab tab = Mockito.mock(Tab.class);
        doReturn(url).when(tab).getUrlString();
        UserDataHost udh = new UserDataHost();
        udh.setUserData(ReparentingTask.class, mTask);
        doReturn(udh).when(tab).getUserDataHost();
        doReturn(id).when(tab).getId();

        int index;
        if (incognito) {
            mIncognitoTabModel.addTab(tab, -1, TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabCreationState.LIVE_IN_FOREGROUND);
            index = mIncognitoTabModel.indexOf(tab);
        } else {
            mTabModel.addTab(tab, -1, TabLaunchType.FROM_BROWSER_ACTIONS,
                    TabCreationState.LIVE_IN_FOREGROUND);
            index = mTabModel.indexOf(tab);
        }
        mTabIndexMapping.put(tab, index);

        return tab;
    }

    private Tab createAndAddMockTab(int id, boolean incognito) {
        return createAndAddMockTab(id, incognito, "https://google.com");
    }

    private Tab getForegroundTab() {
        return mForegroundTab;
    }
}

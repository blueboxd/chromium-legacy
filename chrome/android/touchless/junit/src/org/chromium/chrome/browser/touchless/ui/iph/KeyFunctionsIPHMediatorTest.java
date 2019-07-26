// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the {@link KeyFunctionsIPHMediator} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowPostTask.class})
public class KeyFunctionsIPHMediatorTest {
    private static final String SAMPLE_URL = "https://google.com/chrome/";

    private PropertyModel mModel;
    private ArgumentCaptor<TabObserver> mTabObserver;
    private Tab mTab;
    private ChromePreferenceManager mChromePreferenceManager;
    private Tracker mTracker;
    private KeyFunctionsIPHMediator mKeyFunctionsIPHMediator;

    @Before
    public void setUp() {
        mTab = mock(Tab.class);
        mChromePreferenceManager = mock(ChromePreferenceManager.class);
        mTracker = mock(Tracker.class);
        mTabObserver = ArgumentCaptor.forClass(TabObserver.class);
        mModel = spy(new PropertyModel.Builder(KeyFunctionsIPHProperties.ALL_KEYS).build());
        ActivityTabProvider activityTabProvider = spy(ActivityTabProvider.class);

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.acquireDisplayLock()).thenReturn(mock(Tracker.DisplayLockHandle.class));
        when(activityTabProvider.get()).thenReturn(mTab);
        when(mChromePreferenceManager.readInt(
                     ChromePreferenceManager.TOUCHLESS_BROWSING_SESSION_COUNT))
                .thenReturn(0);
        mKeyFunctionsIPHMediator = spy(new KeyFunctionsIPHMediator(
                mModel, activityTabProvider, mChromePreferenceManager, mTracker));
        verify(mTab).addObserver(mTabObserver.capture());
    }

    @Test
    public void visibilityTest() {
        when(mTab.getUrl()).thenReturn(SAMPLE_URL);
        mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE), false);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);

        for (int i = 1; i < KeyFunctionsIPHMediator.INTRODUCTORY_PAGE_LOAD_CYCLE; i++) {
            mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
            Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);
        }

        mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        when(mChromePreferenceManager.readInt(
                     ChromePreferenceManager.TOUCHLESS_BROWSING_SESSION_COUNT))
                .thenReturn(KeyFunctionsIPHMediator.INTRODUCTORY_SESSIONS + 1);
        for (int i = 0; i <= KeyFunctionsIPHMediator.INTRODUCTORY_PAGE_LOAD_CYCLE; i++) {
            mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
            Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);
        }
    }

    @Test
    public void cursorVisibilityToggleTest() {
        // Toggling cursor mode does not trigger the IPH, but should update the model.
        mKeyFunctionsIPHMediator.onFallbackCursorModeToggled(true);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE), true);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);

        mKeyFunctionsIPHMediator.onFallbackCursorModeToggled(false);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE), false);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);
    }

    @Test
    public void otherIPHShowingTest() {
        // Toggling cursor mode does not trigger the IPH.
        when(mTracker.acquireDisplayLock()).thenReturn(null);
        mKeyFunctionsIPHMediator.onFallbackCursorModeToggled(true);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);

        when(mTracker.acquireDisplayLock()).thenReturn(mock(Tracker.DisplayLockHandle.class));
        mKeyFunctionsIPHMediator.onFallbackCursorModeToggled(false);
        Assert.assertEquals(mModel.get(KeyFunctionsIPHProperties.IS_VISIBLE), false);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }
}

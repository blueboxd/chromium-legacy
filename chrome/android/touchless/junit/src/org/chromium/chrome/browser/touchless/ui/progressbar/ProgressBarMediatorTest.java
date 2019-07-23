// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.touchless.TouchlessUrlUtilities;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the {@link ProgressBarMediator} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowPostTask.class})
public class ProgressBarMediatorTest {
    private static final String SAMPLE_URL = "https://www.google.com/chrome";
    private static final String SAMPLE_URL_SHORT = "google.com";

    private ProgressBarMediator mProgressBarMediator;
    private PropertyModel mModel;
    private ArgumentCaptor<TabObserver> mTabObserver;
    private Tab mTab;

    @Before
    public void setUp() {
        TouchlessUrlUtilities.setUrlForDisplayForTest(SAMPLE_URL_SHORT);
        mModel = new PropertyModel.Builder(ProgressBarProperties.ALL_KEYS).build();
        mTabObserver = ArgumentCaptor.forClass(TabObserver.class);
        mTab = mock(Tab.class);
        ActivityTabProvider activityTabProvider = spy(ActivityTabProvider.class);
        when(activityTabProvider.get()).thenReturn(mTab);
        mProgressBarMediator = new ProgressBarMediator(mModel, activityTabProvider);
        verify(mTab).addObserver(mTabObserver.capture());
    }

    @After
    public void tearDown() {
        TouchlessUrlUtilities.setUrlForDisplayForTest(null);
    }

    @Test
    public void visibilityTest() {
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), false);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        when(navigationHandle.isInMainFrame()).thenReturn(true);
        when(navigationHandle.getUrl()).thenReturn(SAMPLE_URL);
        mTabObserver.getValue().onDidStartNavigation(mTab, navigationHandle);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // Mock page load finish, but not display timeout. The progress bar should still be visible.
        mTabObserver.getValue().onLoadStopped(mTab, true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // Mock display timeout. The progress bar should not be visible at this point.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // The progress bar should be shown on activity start.
        mProgressBarMediator.onActivityStart();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // And should disappear after the timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // The progress bar should be disabled for native pages.
        when(navigationHandle.getUrl()).thenReturn(UrlConstants.NTP_URL);
        mTabObserver.getValue().onDidStartNavigation(mTab, navigationHandle);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), false);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // Mock display timeout before page load finish.
        when(navigationHandle.getUrl()).thenReturn(SAMPLE_URL);
        mTabObserver.getValue().onDidStartNavigation(mTab, navigationHandle);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);
    }

    @Test
    public void progressAndUrlTest() {
        Assert.assertNull(mModel.get(ProgressBarProperties.URL));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.isInMainFrame()).thenReturn(true);
        when(navigationHandle.getUrl()).thenReturn(SAMPLE_URL);
        when(mTab.getUrl()).thenReturn(SAMPLE_URL);
        mTabObserver.getValue().onDidStartNavigation(mTab, navigationHandle);

        mTabObserver.getValue().onUrlUpdated(mTab);
        Assert.assertEquals(mModel.get(ProgressBarProperties.URL), SAMPLE_URL_SHORT);

        mTabObserver.getValue().onLoadProgressChanged(mTab, 10);
        Assert.assertEquals(mModel.get(ProgressBarProperties.PROGRESS_FRACTION), .1, .001);

        // Progress and URL should not be updated for native pages.
        when(mTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        mTabObserver.getValue().onLoadProgressChanged(mTab, 20);
        mTabObserver.getValue().onUrlUpdated(mTab);
        Assert.assertEquals(mModel.get(ProgressBarProperties.PROGRESS_FRACTION), .1, .001);
        Assert.assertEquals(mModel.get(ProgressBarProperties.URL), SAMPLE_URL_SHORT);
    }
}

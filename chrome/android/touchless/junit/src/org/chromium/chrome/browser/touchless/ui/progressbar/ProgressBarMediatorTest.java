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
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the {@link ProgressBarMediator} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class, ShadowPostTask.class})
public class ProgressBarMediatorTest {
    private static final String SAMPLE_URL = "https://www.google.com/chrome";
    private static final String SAMPLE_URL_SHORT = "google.com";

    private PropertyModel mModel;
    private ArgumentCaptor<TabObserver> mTabObserver;
    private Tab mTab;

    @Before
    public void setUp() {
        ShadowUrlUtilities.setTestImpl(new ShadowUrlUtilities.TestImpl() {
            @Override
            public String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
                return SAMPLE_URL_SHORT;
            }
        });
    }

    private ProgressBarMediator initProgressBarMediator() {
        mModel = spy(new PropertyModel.Builder(ProgressBarProperties.ALL_KEYS).build());
        mTabObserver = ArgumentCaptor.forClass(TabObserver.class);
        mTab = mock(Tab.class);
        ActivityTabProvider activityTabProvider = spy(ActivityTabProvider.class);
        when(activityTabProvider.get()).thenReturn(mTab);
        ProgressBarMediator progressBarMediator =
                new ProgressBarMediator(mModel, activityTabProvider);
        verify(mTab).addObserver(mTabObserver.capture());
        return progressBarMediator;
    }

    @Test
    public void visibilityTest() {
        ProgressBarMediator progressBarMediator = initProgressBarMediator();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), false);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // Mock page load finish, but not display timeout. The progress bar should still be visible.
        mTabObserver.getValue().onPageLoadFinished(mTab, SAMPLE_URL);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // Mock display timeout. The progress bar should not be visible at this point.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // The progress bar should be shown on activity resume.
        progressBarMediator.onActivityResume();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // And should disappear after the timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // The progress bar should be disabled for native pages.
        mTabObserver.getValue().onPageLoadStarted(mTab, UrlConstants.NTP_URL);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), false);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);

        // Mock key press before timeout and page load finish.
        mTabObserver.getValue().onPageLoadStarted(mTab, SAMPLE_URL);
        progressBarMediator.onKeyEvent();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), true);

        // Mock display timeout before page load finish, and after key press.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_ENABLED), true);
        Assert.assertEquals(mModel.get(ProgressBarProperties.IS_VISIBLE), false);
    }

    @Test
    public void progressAndUrlTest() {
        initProgressBarMediator();
        Assert.assertNull(mModel.get(ProgressBarProperties.URL));

        when(mTab.getUrl()).thenReturn(SAMPLE_URL);
        mTabObserver.getValue().onLoadProgressChanged(mTab, 10);
        Assert.assertEquals(mModel.get(ProgressBarProperties.PROGRESS_FRACTION), .1, .001);
        Assert.assertEquals(mModel.get(ProgressBarProperties.URL), SAMPLE_URL_SHORT);

        // Progress and URL should not be updated for native pages.
        when(mTab.getUrl()).thenReturn(UrlConstants.NTP_URL);
        mTabObserver.getValue().onLoadProgressChanged(mTab, 20);
        Assert.assertEquals(mModel.get(ProgressBarProperties.PROGRESS_FRACTION), .1, .001);
        Assert.assertEquals(mModel.get(ProgressBarProperties.URL), SAMPLE_URL_SHORT);
    }

    @After
    public void tearDown() {
        ShadowUrlUtilities.reset();
    }
}

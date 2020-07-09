// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.view.ContentView;

/**
 * Unit tests for {@link ChromeFullscreenManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenManagerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int EXTRA_TOP_CONTROL_HEIGHT = 20;

    @Mock
    private Activity mActivity;
    @Mock
    private ControlContainer mControlContainer;
    @Mock
    private View mContainerView;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private android.content.res.Resources mResources;
    @Mock
    private BrowserControlsStateProvider.Observer mBrowserControlsStateProviderObserver;
    @Mock
    private Tab mTab;
    @Mock
    private ContentView mContentView;

    private UserDataHost mUserDataHost = new UserDataHost();
    private ChromeFullscreenManager mFullscreenManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.control_container_height))
                .thenReturn(TOOLBAR_HEIGHT);
        when(mControlContainer.getView()).thenReturn(mContainerView);
        when(mContainerView.getVisibility()).thenReturn(View.VISIBLE);
        when(mTab.isUserInteractable()).thenReturn(true);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getContentView()).thenReturn(mContentView);
        doNothing().when(mContentView).removeOnHierarchyChangeListener(any());
        doNothing().when(mContentView).removeOnSystemUiVisibilityChangeListener(any());
        doNothing().when(mContentView).addOnHierarchyChangeListener(any());
        doNothing().when(mContentView).addOnSystemUiVisibilityChangeListener(any());

        ChromeFullscreenManager fullscreenManager = new ChromeFullscreenManager(
                mActivity, ChromeFullscreenManager.ControlsPosition.TOP);
        mFullscreenManager = spy(fullscreenManager);
        mFullscreenManager.initialize(mControlContainer, mActivityTabProvider, mTabModelSelector,
                R.dimen.control_container_height);
        mFullscreenManager.addObserver(mBrowserControlsStateProviderObserver);
        when(mFullscreenManager.getTab()).thenReturn(mTab);
    }

    @Test
    public void testInitialTopControlsHeight() {
        assertEquals("Wrong initial top controls height.", TOOLBAR_HEIGHT,
                mFullscreenManager.getTopControlsHeight());
    }

    @Test
    public void testListenersNotifiedOfTopControlsHeightChange() {
        final int topControlsHeight = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        final int topControlsMinHeight = EXTRA_TOP_CONTROL_HEIGHT;
        mFullscreenManager.setTopControlsHeight(topControlsHeight, topControlsMinHeight);
        verify(mBrowserControlsStateProviderObserver)
                .onTopControlsHeightChanged(topControlsHeight, topControlsMinHeight);
    }

    @Test
    public void testBrowserDrivenHeightIncreaseAnimation() {
        final int topControlsHeight = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        final int topControlsMinHeight = EXTRA_TOP_CONTROL_HEIGHT;

        // Simulate that we can't animate native browser controls.
        when(mFullscreenManager.getTab()).thenReturn(null);
        mFullscreenManager.setAnimateBrowserControlsHeightChanges(true);
        mFullscreenManager.setTopControlsHeight(topControlsHeight, topControlsMinHeight);

        assertNotEquals("Min-height offset shouldn't immediately change.", topControlsMinHeight,
                mFullscreenManager.getTopControlsMinHeightOffset());
        assertNotNull("Animator should be initialized.",
                mFullscreenManager.getControlsAnimatorForTesting());

        for (long time = 50; time < mFullscreenManager.getControlsAnimationDurationMsForTesting();
                time += 50) {
            int previousMinHeightOffset = mFullscreenManager.getTopControlsMinHeightOffset();
            int previousContentOffset = mFullscreenManager.getContentOffset();
            mFullscreenManager.getControlsAnimatorForTesting().setCurrentPlayTime(time);
            assertThat(mFullscreenManager.getTopControlsMinHeightOffset(),
                    greaterThan(previousMinHeightOffset));
            assertThat(mFullscreenManager.getContentOffset(), greaterThan(previousContentOffset));
        }

        mFullscreenManager.getControlsAnimatorForTesting().end();
        assertEquals("Min-height offset should be equal to min-height after animation.",
                mFullscreenManager.getTopControlsMinHeightOffset(), topControlsMinHeight);
        assertEquals("Content offset should be equal to controls height after animation.",
                mFullscreenManager.getContentOffset(), topControlsHeight);
        assertNull(mFullscreenManager.getControlsAnimatorForTesting());
    }

    @Test
    public void testBrowserDrivenHeightDecreaseAnimation() {
        // Simulate that we can't animate native browser controls.
        when(mFullscreenManager.getTab()).thenReturn(null);

        mFullscreenManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        mFullscreenManager.setAnimateBrowserControlsHeightChanges(true);
        mFullscreenManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        assertNotEquals("Min-height offset shouldn't immediately change.", 0,
                mFullscreenManager.getTopControlsMinHeightOffset());
        assertNotNull("Animator should be initialized.",
                mFullscreenManager.getControlsAnimatorForTesting());

        for (long time = 50; time < mFullscreenManager.getControlsAnimationDurationMsForTesting();
                time += 50) {
            int previousMinHeightOffset = mFullscreenManager.getTopControlsMinHeightOffset();
            int previousContentOffset = mFullscreenManager.getContentOffset();
            mFullscreenManager.getControlsAnimatorForTesting().setCurrentPlayTime(time);
            assertThat(mFullscreenManager.getTopControlsMinHeightOffset(),
                    lessThan(previousMinHeightOffset));
            assertThat(mFullscreenManager.getContentOffset(), lessThan(previousContentOffset));
        }

        mFullscreenManager.getControlsAnimatorForTesting().end();
        assertEquals("Min-height offset should be equal to the min-height after animation.",
                mFullscreenManager.getTopControlsMinHeight(),
                mFullscreenManager.getTopControlsMinHeightOffset());
        assertEquals("Content offset should be equal to controls height after animation.",
                mFullscreenManager.getTopControlsHeight(), mFullscreenManager.getContentOffset());
        assertNull(mFullscreenManager.getControlsAnimatorForTesting());
    }

    @Test
    public void testChangeTopHeightWithoutAnimation_Browser() {
        // Simulate that we can't animate native browser controls.
        when(mFullscreenManager.getTab()).thenReturn(null);

        // Increase the height.
        mFullscreenManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        verify(mFullscreenManager).showAndroidControls(false);
        assertEquals("Controls should be fully shown after changing the height.",
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, mFullscreenManager.getContentOffset());
        assertEquals("Controls should be fully shown after changing the height.", 0,
                mFullscreenManager.getTopControlOffset());
        assertEquals("Min-height offset should be equal to the min-height after height changes.",
                EXTRA_TOP_CONTROL_HEIGHT, mFullscreenManager.getTopControlsMinHeightOffset());

        // Decrease the height.
        mFullscreenManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        // Controls should be fully shown after changing the height.
        verify(mFullscreenManager, times(2)).showAndroidControls(false);
        assertEquals("Controls should be fully shown after changing the height.", TOOLBAR_HEIGHT,
                mFullscreenManager.getContentOffset());
        assertEquals("Controls should be fully shown after changing the height.", 0,
                mFullscreenManager.getTopControlOffset());
        assertEquals("Min-height offset should be equal to the min-height after height changes.", 0,
                mFullscreenManager.getTopControlsMinHeightOffset());
    }

    @Test
    public void testChangeTopHeightWithoutAnimation_Native() {
        int contentOffset = mFullscreenManager.getContentOffset();
        int controlOffset = mFullscreenManager.getTopControlOffset();
        int minHeightOffset = mFullscreenManager.getTopControlsMinHeightOffset();

        // Increase the height.
        mFullscreenManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        // Controls visibility and offsets should be managed by native.
        verify(mFullscreenManager, never()).showAndroidControls(anyBoolean());
        assertEquals("Content offset should have the initial value before round-trip to native.",
                contentOffset, mFullscreenManager.getContentOffset());
        assertEquals("Controls offset should have the initial value before round-trip to native.",
                controlOffset, mFullscreenManager.getTopControlOffset());
        assertEquals("Min-height offset should have the initial value before round-trip to native.",
                minHeightOffset, mFullscreenManager.getTopControlsMinHeightOffset());

        verify(mBrowserControlsStateProviderObserver)
                .onTopControlsHeightChanged(
                        TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        contentOffset = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        controlOffset = 0;
        minHeightOffset = EXTRA_TOP_CONTROL_HEIGHT;

        // Simulate the offset coming from cc::BrowserControlsOffsetManager.
        mFullscreenManager.getTabControlsObserverForTesting().onBrowserControlsOffsetChanged(
                mTab, controlOffset, 0, contentOffset, minHeightOffset, 0);

        // Decrease the height.
        mFullscreenManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        // Controls visibility and offsets should be managed by native.
        verify(mFullscreenManager, never()).showAndroidControls(anyBoolean());
        assertEquals("Controls should be fully shown after getting the offsets from native.",
                contentOffset, mFullscreenManager.getContentOffset());
        assertEquals("Controls should be fully shown after getting the offsets from native.",
                controlOffset, mFullscreenManager.getTopControlOffset());
        assertEquals("Min-height offset should be equal to the min-height"
                        + " after getting the offsets from native.",
                minHeightOffset, mFullscreenManager.getTopControlsMinHeightOffset());

        verify(mBrowserControlsStateProviderObserver).onTopControlsHeightChanged(TOOLBAR_HEIGHT, 0);
    }
}

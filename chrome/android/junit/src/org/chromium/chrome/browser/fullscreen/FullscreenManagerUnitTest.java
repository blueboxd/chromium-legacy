// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
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
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
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
    private boolean mControlsResizeView;

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
        mFullscreenManager.setViewportSizeDelegate(() -> {
            mControlsResizeView = BrowserControlsUtils.controlsResizeView(mFullscreenManager);
        });
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

    // controlsResizeView tests ---
    // For these tests, we will simulate the scrolls assuming we either completely show or hide (or
    // scroll until the min-height) the controls and don't leave at in-between positions. The reason
    // is that ChromeFullscreenManager only flips the mControlsResizeView bit if the controls are
    // idle, meaning they're at the min-height or fully shown. Making sure the controls snap to
    // these two positions is not CFM's responsibility as it's handled in native code by compositor
    // or blink.

    @Test
    public void testControlsResizeViewChanges() {
        // Let's use simpler numbers for this test.
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mFullscreenManager.getTabControlsObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mFullscreenManager));
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mControlsResizeView);

        // Scroll to fully hidden.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -100,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 0,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertFalse("ControlsResizeView is true,"
                        + " but it should be false when the controls are hidden.",
                mControlsResizeView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 0, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are hidden when they should be fully visible.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mFullscreenManager));
        // #controlsResizeView should be flipped back to true.
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mControlsResizeView);
    }

    @Test
    public void testControlsResizeViewChangesWithMinHeight() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mFullscreenManager.getTabControlsObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);
        mFullscreenManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        // Initially, the controls should be fully visible.
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mFullscreenManager));
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mControlsResizeView);

        // Scroll all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("Browser controls aren't at min-height.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertFalse("ControlsResizeView is true,"
                        + " but it should be false when the controls are at min-height.",
                mControlsResizeView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("Browser controls are at min-height when they should be fully visible.",
                mFullscreenManager.areBrowserControlsAtMinHeight());
        assertTrue("Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mFullscreenManager));
        // #controlsResizeView should be flipped back to true.
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mControlsResizeView);
    }

    @Test
    public void testControlsResizeViewWhenControlsAreNotIdle() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mFullscreenManager.getTabControlsObserverForTesting();

        mFullscreenManager.setTopControlsHeight(topHeight, topMinHeight);
        mFullscreenManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ 0,
                /*bottomControlsOffsetY*/ 0, /*contentOffsetY*/ 100,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue("ControlsResizeView is false,"
                        + " but it should be true when the controls are fully visible.",
                mControlsResizeView);

        // Scroll a little hide the controls partially.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -25,
                /*bottomControlsOffsetY*/ 20, /*contentOffsetY*/ 75,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertTrue(
                "ControlsResizeView is false, but it should still be true.", mControlsResizeView);

        // Scroll controls all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -75,
                /*bottomControlsOffsetY*/ 60, /*contentOffsetY*/ 25,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse("ControlsResizeView is true,"
                        + " but it should've flipped to false since the controls are idle now.",
                mControlsResizeView);

        // Scroll controls to show a little more.
        tabControlsObserver.onBrowserControlsOffsetChanged(mTab, /*topControlsOffsetY*/ -50,
                /*bottomControlsOffsetY*/ 40, /*contentOffsetY*/ 50,
                /*topControlsMinHeightOffsetY*/ 25, /*bottomControlsMinHeightOffsetY*/ 0);
        assertFalse(
                "ControlsResizeView is true, but it should still be false.", mControlsResizeView);
    }

    // --- controlsResizeView tests
}

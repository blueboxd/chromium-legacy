// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link StatusIndicatorMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusIndicatorMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    ChromeFullscreenManager mFullscreenManager;

    private PropertyModel mModel;
    private StatusIndicatorMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                         .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.GONE)
                         .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                         .build();
        mMediator = new StatusIndicatorMediator(mModel, mFullscreenManager);
    }

    @Test
    public void testHeightChangeAddsListener() {
        //
        mMediator.onStatusIndicatorHeightChanged(70);
        verify(mFullscreenManager).addListener(mMediator);
    }

    @Test
    public void testHeightChangeDoesNotRemoveListenerImmediately() {
        // Show the status indicator.
        mMediator.onStatusIndicatorHeightChanged(70);
        mMediator.onControlsOffsetChanged(0, 70, 0, 0, false);

        // Now, hide it. Listener shouldn't be removed.
        mMediator.onStatusIndicatorHeightChanged(0);
        verify(mFullscreenManager, never()).removeListener(mMediator);

        // Once the hiding animation is done...
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false);
        // The listener should be removed.
        verify(mFullscreenManager).removeListener(mMediator);
    }

    @Test
    public void testHeightChangeToZeroMakesAndroidViewGone() {
        // Show the status indicator.
        mMediator.onStatusIndicatorHeightChanged(70);
        mMediator.onControlsOffsetChanged(0, 70, 0, 0, false);
        // The Android view should be visible at this point.
        assertThat(mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY),
                equalTo(View.VISIBLE));
        // Now hide it.
        mMediator.onStatusIndicatorHeightChanged(0);
        // The hiding animation...
        mMediator.onControlsOffsetChanged(0, 30, 0, 0, false);
        // Android view will be gone once the animation starts.
        assertThat(
                mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY), equalTo(View.GONE));
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false);
        // Shouldn't make the Android view invisible. It should stay gone.
        assertThat(
                mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY), equalTo(View.GONE));
    }

    @Test
    public void testOffsetChangeUpdatesVisibility() {
        // Initially, the Android view should be GONE.
        mMediator.onStatusIndicatorHeightChanged(20);
        assertThat(
                mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY), equalTo(View.GONE));
        // Assume the status indicator is completely hidden.
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false);
        assertThat(mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY),
                equalTo(View.INVISIBLE));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is partially showing.
        mMediator.onControlsOffsetChanged(0, 10, 0, 0, false);
        assertThat(mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY),
                equalTo(View.INVISIBLE));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is fully showing, 20px.
        mMediator.onControlsOffsetChanged(0, 20, 0, 0, false);
        assertThat(mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY),
                equalTo(View.VISIBLE));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Hide again.
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false);
        assertThat(mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY),
                equalTo(View.INVISIBLE));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
    }
}

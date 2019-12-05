// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link VisibilityMonitor}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VisibilityMonitorTest {
    private static final Configuration CONFIGURATION =
            new Configuration.Builder().put(ConfigKey.VIEW_LOG_THRESHOLD, .50).build();
    private static final int VIEW_HEIGHT = 100;
    private static final int VIEW_WIDTH = 50;

    @Mock
    private VisibilityListener mVisibilityListener;

    private Activity mContext;
    private ViewGroup mParentView;
    private View mView;
    private VisibilityMonitor mVisibilityMonitor;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.setupActivity(Activity.class);

        setUpViews(((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, VIEW_HEIGHT);
            return true;
        }));
    }

    @Test
    public void testPreDraw_notifiesListener() {
        mContext.setContentView(mParentView);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener).onViewVisible();
    }

    @Test
    public void testPreDraw_notifiesListenerIfVisibleViewHeightIsAtThreshold() {
        setUpViews((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, 50);
            return true;
        });
        mContext.setContentView(mParentView);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfAlreadyVisible() {
        mContext.setContentView(mParentView);
        mVisibilityMonitor.onPreDraw();
        reset(mVisibilityListener);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfVisibleViewHeightIsBelowThreshold() {
        setUpViews((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, 49);
            return true;
        });
        mContext.setContentView(mParentView);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfViewNotAttached() {
        setUpViews((child, r, offset) -> false);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfParentIsNull() {
        mVisibilityMonitor = new VisibilityMonitor(new FrameLayout(mContext), CONFIGURATION);
        mVisibilityMonitor.setListener(null);

        mVisibilityMonitor.onPreDraw();

        verify(mVisibilityListener, never()).onViewVisible();
    }

    private void setUpViews(ChildVisibleRectMock childVisibleRectMock) {
        mView = new FrameLayout(mContext);
        mView.setLayoutParams(new LayoutParams(VIEW_WIDTH, VIEW_HEIGHT));
        mParentView = new FrameLayout(mContext) {
            @Override
            public boolean getChildVisibleRect(View child, Rect r, Point offset) {
                return childVisibleRectMock.getChildVisibleRect(child, r, offset);
            }
        };

        mParentView.addView(mView);
        mVisibilityMonitor = new VisibilityMonitor(mView, CONFIGURATION);
        mVisibilityMonitor.setListener(mVisibilityListener);
    }

    interface ChildVisibleRectMock {
        boolean getChildVisibleRect(View child, Rect r, Point offset);
    }
}

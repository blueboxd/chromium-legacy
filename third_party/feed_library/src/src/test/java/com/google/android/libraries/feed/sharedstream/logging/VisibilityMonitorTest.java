// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

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

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link VisibilityMonitor}. */
@RunWith(RobolectricTestRunner.class)
public class VisibilityMonitorTest {
    private static final Configuration CONFIGURATION =
            new Configuration.Builder().put(ConfigKey.VIEW_LOG_THRESHOLD, .50).build();
    private static final int VIEW_HEIGHT = 100;
    private static final int VIEW_WIDTH = 50;

    @Mock
    private VisibilityListener visibilityListener;

    private Activity context;
    private ViewGroup parentView;
    private View view;
    private VisibilityMonitor visibilityMonitor;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.setupActivity(Activity.class);

        setUpViews(((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, VIEW_HEIGHT);
            return true;
        }));
    }

    @Test
    public void testPreDraw_notifiesListener() {
        context.setContentView(parentView);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener).onViewVisible();
    }

    @Test
    public void testPreDraw_notifiesListenerIfVisibleViewHeightIsAtThreshold() {
        setUpViews((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, 50);
            return true;
        });
        context.setContentView(parentView);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfAlreadyVisible() {
        context.setContentView(parentView);
        visibilityMonitor.onPreDraw();
        reset(visibilityListener);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfVisibleViewHeightIsBelowThreshold() {
        setUpViews((child, r, offset) -> {
            r.set(0, 0, VIEW_WIDTH, 49);
            return true;
        });
        context.setContentView(parentView);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfViewNotAttached() {
        setUpViews((child, r, offset) -> false);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener, never()).onViewVisible();
    }

    @Test
    public void testPreDraw_doesNotNotifyListenerIfParentIsNull() {
        visibilityMonitor = new VisibilityMonitor(new FrameLayout(context), CONFIGURATION);
        visibilityMonitor.setListener(null);

        visibilityMonitor.onPreDraw();

        verify(visibilityListener, never()).onViewVisible();
    }

    private void setUpViews(ChildVisibleRectMock childVisibleRectMock) {
        view = new FrameLayout(context);
        view.setLayoutParams(new LayoutParams(VIEW_WIDTH, VIEW_HEIGHT));
        parentView = new FrameLayout(context) {
            @Override
            public boolean getChildVisibleRect(View child, Rect r, Point offset) {
                return childVisibleRectMock.getChildVisibleRect(child, r, offset);
            }
        };

        parentView.addView(view);
        visibilityMonitor = new VisibilityMonitor(view, CONFIGURATION);
        visibilityMonitor.setListener(visibilityListener);
    }

    interface ChildVisibleRectMock {
        boolean getChildVisibleRect(View child, Rect r, Point offset);
    }
}

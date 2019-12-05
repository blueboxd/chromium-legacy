// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.graphics.Paint;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders.Edges;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link BorderDrawable}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BorderDrawableTest {
    private static final float[] ZERO_RADII = new float[] {0, 0, 0, 0, 0, 0, 0, 0};
    private static final boolean LEFT_TO_RIGHT = false;
    private static final boolean RIGHT_TO_LEFT = true;

    private static final int COLOR = 0xFFFF0000;
    private static final int WIDTH_DP = 8;
    private int mWidthPx;

    private static final Borders DEFAULT_BORDER =
            Borders.newBuilder().setWidth(WIDTH_DP).setColor(COLOR).build();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mWidthPx = (int) LayoutUtils.dpToPx(WIDTH_DP, mContext);
    }

    @Test
    public void testBorders_setsPaintParams() {
        BorderDrawable borderDrawable =
                new BorderDrawable(mContext, DEFAULT_BORDER, ZERO_RADII, LEFT_TO_RIGHT);

        assertThat(borderDrawable.getPaint().getStyle()).isEqualTo(Paint.Style.STROKE);
        assertThat(borderDrawable.getPaint().getStrokeWidth()).isEqualTo((float) mWidthPx * 2);
        assertThat(borderDrawable.getPaint().getColor()).isEqualTo(COLOR);
    }

    @Test
    public void testBorders_allSides_default() {
        BorderDrawable borderDrawable =
                new BorderDrawable(mContext, DEFAULT_BORDER, ZERO_RADII, LEFT_TO_RIGHT);

        assertThat(borderDrawable.getPaint().getStyle()).isEqualTo(Paint.Style.STROKE);
        assertThat(borderDrawable.getPaint().getStrokeWidth()).isEqualTo((float) mWidthPx * 2);
        assertThat(borderDrawable.getPaint().getColor()).isEqualTo(COLOR);

        borderDrawable.setBounds(1, 2, 3, 4);
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1, 2, 3, 4));
    }

    @Test
    public void testBorders_allSides_explicit() {
        BorderDrawable borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.BOTTOM.getNumber() | Edges.TOP.getNumber()
                                | Edges.START.getNumber() | Edges.END.getNumber())
                        .build(),
                ZERO_RADII, LEFT_TO_RIGHT);

        borderDrawable.setBounds(1, 2, 3, 4);
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1, 2, 3, 4));
    }

    @Test
    public void testBorders_topLeft() {
        BorderDrawable borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.TOP.getNumber() | Edges.START.getNumber())
                        .build(),
                ZERO_RADII, LEFT_TO_RIGHT);

        borderDrawable.setBounds(1, 2, 3, 4);
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1, 2, 3 + mWidthPx, 4 + mWidthPx));
    }

    @Test
    public void testBorders_bottomRight() {
        BorderDrawable borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.BOTTOM.getNumber() | Edges.END.getNumber())
                        .build(),
                ZERO_RADII, LEFT_TO_RIGHT);

        borderDrawable.setBounds(1, 2, 3, 4);
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1 - mWidthPx, 2 - mWidthPx, 3, 4));
    }

    @Test
    public void testBorders_someSides_RtL() {
        BorderDrawable borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.TOP.getNumber() | Edges.BOTTOM.getNumber()
                                | Edges.END.getNumber())
                        .build(),
                ZERO_RADII, RIGHT_TO_LEFT);

        borderDrawable.setBounds(1, 2, 3, 4);
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1, 2, 3 + mWidthPx, 4));

        borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.TOP.getNumber() | Edges.BOTTOM.getNumber()
                                | Edges.START.getNumber())
                        .build(),
                ZERO_RADII, RIGHT_TO_LEFT);

        borderDrawable.setBounds(1, 2, 3, 4);
        bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1 - mWidthPx, 2, 3, 4));
    }

    @Test
    public void testSetBoundsRect() {
        BorderDrawable borderDrawable = new BorderDrawable(mContext,
                DEFAULT_BORDER.toBuilder()
                        .setBitmask(Edges.TOP.getNumber() | Edges.END.getNumber())
                        .build(),
                ZERO_RADII, LEFT_TO_RIGHT);

        borderDrawable.setBounds(new Rect(1, 2, 3, 4));
        Rect bounds = borderDrawable.getBounds();
        assertThat(bounds).isEqualTo(new Rect(1 - mWidthPx, 2, 3, 4 + mWidthPx));
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.piet.ui.GridRowView.LayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for the {@link GridRowView}. */
@RunWith(RobolectricTestRunner.class)
public class GridRowViewTest {
    private static final int WIDTH = 100;
    private static final int WRAP_CONTENT_WIDTH = 75;
    private static final int COLLAPSIBLE_WIDTH = 80;
    private static final int HEIGHT = 1234;
    private static final int LARGE_HEIGHT = 10000;

    private int unspecifiedSpec;

    private Context context;

    private TestView collapsibleView;
    private TestView collapsibleView2;
    private TestView wrapContentView;
    private TestView widthView;
    private TestView weightView;
    private TestView weightView2;
    private TestView goneView;

    private GridRowView gridRowView;

    @Before
    public void setUp() {
        context = Robolectric.buildActivity(Activity.class).get();

        unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        gridRowView = new GridRowView(context, Suppliers.of(false));

        collapsibleView = new TestView(context, COLLAPSIBLE_WIDTH, HEIGHT);
        collapsibleView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, true));

        collapsibleView2 = new TestView(context, COLLAPSIBLE_WIDTH, HEIGHT);
        collapsibleView2.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, true));

        wrapContentView = new TestView(context, WRAP_CONTENT_WIDTH, HEIGHT);
        wrapContentView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, false));
        // Not sure how to get the wrap content width

        widthView = new TestView(context, WIDTH, HEIGHT);
        widthView.setLayoutParams(new LayoutParams(WIDTH, HEIGHT, 0.0f, false));

        weightView = new TestView(context, 0, HEIGHT);
        weightView.setLayoutParams(new LayoutParams(0, HEIGHT, 1.0f, false));

        weightView2 = new TestView(context, 0, HEIGHT);
        weightView2.setLayoutParams(new LayoutParams(0, HEIGHT, 1.0f, false));

        goneView = new TestView(context, WIDTH, LARGE_HEIGHT);
        goneView.setMinimumHeight(LARGE_HEIGHT);
        goneView.setMinimumWidth(WIDTH);
        goneView.setVisibility(View.GONE);
    }

    @Test
    public void testOnlyHorizontal() {
        assertThat(gridRowView.getOrientation()).isEqualTo(LinearLayout.HORIZONTAL);
        assertThatRunnable(() -> gridRowView.setOrientation(LinearLayout.VERTICAL))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("GridRowView can only be horizontal");
    }

    @Test
    public void testCollapsibleIgnoresWeight() {
        LayoutParams params = new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, true);
        assertThat(params.getIsCollapsible()).isTrue();

        params = new LayoutParams(123, HEIGHT, 0.0f, true);
        assertThat(params.getIsCollapsible()).isTrue();

        params = new LayoutParams(0, HEIGHT, 1.0f, true);
        assertThat(params.getIsCollapsible()).isFalse();
    }

    @Test
    public void testGenerateDefaultLayoutParams() {
        LayoutParams layoutParams = gridRowView.generateDefaultLayoutParams();
        assertThat(layoutParams.width).isEqualTo(0);
        assertThat(layoutParams.height).isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(layoutParams.weight).isEqualTo(1.0f);
        assertThat(layoutParams.getIsCollapsible()).isFalse();
    }

    @Test
    public void testGenerateLayoutParams() {
        LayoutParams result;

        // not LinearLayout
        ViewGroup.LayoutParams viewGroupLayoutParams = new ViewGroup.LayoutParams(12, 34);
        result = gridRowView.generateLayoutParams(viewGroupLayoutParams);
        assertThat(result.width).isEqualTo(12);
        assertThat(result.height).isEqualTo(34);

        // LinearLayout
        LinearLayout.LayoutParams linearLayoutParams = new LinearLayout.LayoutParams(12, 34, 56);
        result = gridRowView.generateLayoutParams(linearLayoutParams);
        assertThat(result.width).isEqualTo(12);
        assertThat(result.height).isEqualTo(34);
        assertThat(result.weight).isEqualTo(56.0f);

        // GridRowView
        GridRowView.LayoutParams gridRowParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, 34, 0, true);
        result = gridRowView.generateLayoutParams(gridRowParams);
        assertThat(result.width).isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(result.height).isEqualTo(34);
        assertThat(result.weight).isEqualTo(0.0f);
        assertThat(result.getIsCollapsible()).isTrue();
    }

    @Test
    public void testCheckLayoutParams() {
        // not LinearLayout
        ViewGroup.LayoutParams viewGroupLayoutParams = new ViewGroup.LayoutParams(12, 34);
        assertThat(gridRowView.checkLayoutParams(viewGroupLayoutParams)).isFalse();

        // LinearLayout
        LinearLayout.LayoutParams linearLayoutParams = new LinearLayout.LayoutParams(12, 34, 56);
        assertThat(gridRowView.checkLayoutParams(linearLayoutParams)).isFalse();

        // GridRowView
        GridRowView.LayoutParams gridRowParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, 34, 0, true);
        assertThat(gridRowView.checkLayoutParams(gridRowParams)).isTrue();
    }

    @Test
    public void testOnMeasure_noCollapsible() {
        // No special processing

        gridRowView.addView(widthView);
        gridRowView.addView(weightView);
        gridRowView.addView(weightView2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(100); // WIDTH
        assertThat(weightView.requestedWidth).isEqualTo(150); // (400 - WIDTH) / 2
        assertThat(weightView2.requestedWidth).isEqualTo(150);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_unspecifiedWidth() {
        // No special processing; collapsible cell is WRAP_CONTENT

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.measure(unspecifiedSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(100); // WIDTH
        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(255); // 100 + 80 + 75
    }

    @Test
    public void testOnMeasure_moreSpaceThanNecessary() {
        // DP, Collapsible, WRAP_CONTENT
        // Extra space is left over

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_notEnoughSpace() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is truncated

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth).isEqualTo(200 - WIDTH - WRAP_CONTENT_WIDTH);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_noSpace() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is completely gone

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(160, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth).isEqualTo(0);
        assertThat(wrapContentView.requestedWidth).isEqualTo(160 - WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(160);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForBoth() {
        // Both cells WRAP_CONTENT and there is space left over

        gridRowView.addView(collapsibleView);
        gridRowView.addView(collapsibleView2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(collapsibleView2.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForFirst() {
        // First cell should WRAP_CONTENT, second cell shrinks

        gridRowView.addView(collapsibleView);
        gridRowView.addView(collapsibleView2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(collapsibleView2.requestedWidth).isEqualTo(100 - COLLAPSIBLE_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForNeither() {
        // First cell fills row, and second gets no space

        gridRowView.addView(collapsibleView);
        gridRowView.addView(collapsibleView2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(collapsibleView.requestedWidth).isEqualTo(50);
        assertThat(collapsibleView2.requestedWidth).isEqualTo(0);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_collapsibleWithWeight_enoughSpace() {
        // Collapsible cell should WRAP_CONTENT, and weight fills the rest

        gridRowView.addView(collapsibleView);
        gridRowView.addView(weightView);
        gridRowView.addView(weightView2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(weightView.requestedWidth).isEqualTo((100 - COLLAPSIBLE_WIDTH) / 2);
        assertThat(weightView2.requestedWidth).isEqualTo((100 - COLLAPSIBLE_WIDTH) / 2);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_collapsibleWithWeight_notEnoughSpace() {
        // Collapsible cell should take up the entire row

        gridRowView.addView(collapsibleView);
        gridRowView.addView(weightView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(collapsibleView.requestedWidth).isEqualTo(50);
        assertThat(weightView.requestedWidth).isEqualTo(0);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_widthCollapsible_enoughSpace() {
        // DP, Collapsible, WRAP
        // Extra space is left over

        collapsibleView.setLayoutParams(new LayoutParams(COLLAPSIBLE_WIDTH, HEIGHT, 0.0f, true));

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_widthCollapsible_notEnoughSpace() {
        // DP, Collapsible, WRAP
        // Collapsible cell is truncated

        collapsibleView.setLayoutParams(new LayoutParams(COLLAPSIBLE_WIDTH, HEIGHT, 0.0f, true));

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth).isEqualTo(200 - WIDTH - WRAP_CONTENT_WIDTH);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_cellThatGetsTallerWithWidth() {
        // Make a cell where width = height and weight = 1 and ensure it behaves with a collapsible

        collapsibleView = new TestView(context, COLLAPSIBLE_WIDTH, 20);
        collapsibleView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, 20, 0.0f, true));

        SquareView squareView = new SquareView(context);
        squareView.setLayoutParams(new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f, false));

        gridRowView.addView(collapsibleView);
        gridRowView.addView(squareView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(130, MeasureSpec.EXACTLY);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        int squareSize = 130 - COLLAPSIBLE_WIDTH;
        assertThat(collapsibleView.requestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(squareView.getMeasuredWidth()).isEqualTo(squareSize);
        assertThat(squareView.getMeasuredHeight()).isEqualTo(squareSize);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(130);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(squareSize);
    }

    @Test
    public void testOnMeasure_wrapContentMatchesCellHeight() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(40);
    }

    @Test
    public void testOnMeasure_doesNotWrapHeightWhenMeasureExactly() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, 100));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_shrinksCellThatWantsToBeTooTall() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView view = new TestView(context, 150, 200);
        view.setLayoutParams(new LayoutParams(150, 200, 0.0f, true));

        gridRowView.addView(view);

        gridRowView.setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, 100));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(100);
        assertThat(view.requestedWidth).isEqualTo(100);
        assertThat(view.requestedHeight).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_respectsMinHeight() {
        // Several different sizes of cells; ensure that height of GridRowView is the min height.

        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        gridRowView.setMinimumHeight(95);

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(95);
    }

    @Test
    public void testOnMeasure_respectsMinHeightUpToAtMost() {
        // Several different sizes of cells; ensure that height of GridRowView is the min height.

        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        gridRowView.setMinimumHeight(1234);

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_respectsChildLayoutParams_weight() {
        // Several different sizes of cells; ensure that width of each cell is set by the style

        int styleWidth = 30;
        LayoutParams cellLayoutParams =
                new LayoutParams(styleWidth, LayoutParams.WRAP_CONTENT, 0.0f, true);
        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(cellLayoutParams);
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(cellLayoutParams);
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(cellLayoutParams);

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(square1.requestedWidth).isEqualTo(styleWidth);
        assertThat(square2.requestedWidth).isEqualTo(styleWidth);
        assertThat(square3.requestedWidth).isEqualTo(styleWidth);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_respectsChildLayoutParams_height() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        int styleHeight = 30;
        LayoutParams cellLayoutParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, styleHeight, 0.0f, true);
        TestView square1 = new TestView(context, 20, 20);
        square1.setLayoutParams(cellLayoutParams);
        TestView square2 = new TestView(context, 40, 40);
        square2.setLayoutParams(cellLayoutParams);
        TestView square3 = new TestView(context, 30, 30);
        square3.setLayoutParams(cellLayoutParams);

        gridRowView.addView(square1);
        gridRowView.addView(square2);
        gridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(square1.requestedHeight).isEqualTo(styleHeight);
        assertThat(square2.requestedHeight).isEqualTo(styleHeight);
        assertThat(square3.requestedHeight).isEqualTo(styleHeight);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(styleHeight);
    }

    @Test
    public void testOnMeasure_rowHeightIsMaxCellHeightWithMargins() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView view1 = new TestView(context, 40, 40);
        view1.setLayoutParams(new LayoutParams(40, 40));

        TestView view2 = new TestView(context, 30, 30);
        LayoutParams params = new LayoutParams(30, 30);
        params.topMargin = 5;
        params.bottomMargin = 15;
        view2.setLayoutParams(params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_cellWithMarginsClippedByHeightConstraint() {
        // Row is 100 high, cell is set to 90 high, but has 5 + 15 margins, so cell is only 80 high.

        TestView view = new TestView(context, 123, 90);
        LayoutParams params = new LayoutParams(123, 90);
        params.topMargin = 5;
        params.bottomMargin = 15;
        view.setLayoutParams(params);

        gridRowView.addView(view);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);

        gridRowView.measure(widthSpec, heightSpec);

        assertThat(view.requestedHeight).isEqualTo(80);
    }

    @Test
    public void testOnMeasure_respectsGridRowPadding() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is truncated

        gridRowView.addView(widthView);
        gridRowView.addView(collapsibleView);
        gridRowView.addView(wrapContentView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        int paddingLeft = 10;
        int paddingTop = 20;
        int paddingRight = 30;
        int paddingBottom = 40;
        gridRowView.setPadding(paddingLeft, paddingTop, paddingRight, paddingBottom);

        int sizeSpec = MeasureSpec.makeMeasureSpec(250, MeasureSpec.EXACTLY);
        gridRowView.measure(sizeSpec, sizeSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(collapsibleView.requestedWidth)
                .isEqualTo(250 - WIDTH - WRAP_CONTENT_WIDTH - paddingLeft - paddingRight);
        assertThat(wrapContentView.requestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(250);

        int cellHeight = 250 - paddingTop - paddingBottom;
        assertThat(widthView.requestedHeight).isEqualTo(cellHeight);
        assertThat(collapsibleView.requestedHeight).isEqualTo(cellHeight);
        assertThat(wrapContentView.requestedHeight).isEqualTo(cellHeight);
        assertThat(gridRowView.getMeasuredHeight()).isEqualTo(250);
    }

    @Test
    public void testOnMeasure_fillParentCellsWithFitContentRow() {
        TestView tallView = new TestView(context, 11, 100);
        tallView.setLayoutParams(new LayoutParams(11, ViewGroup.LayoutParams.MATCH_PARENT));
        TestView shortView = new TestView(context, 12, 20);
        shortView.setLayoutParams(new LayoutParams(12, ViewGroup.LayoutParams.MATCH_PARENT));

        gridRowView.addView(tallView);
        gridRowView.addView(shortView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(new LayoutParams(50, ViewGroup.LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        gridRowView.measure(widthSpec, heightSpec);

        assertThat(tallView.requestedHeight).isEqualTo(100);
        assertThat(shortView.requestedHeight).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_fitContentCellsWithFitContentRow_minHeight() {
        TestView tallView = new TestView(context, 11, 50);
        tallView.setLayoutParams(new LayoutParams(11, ViewGroup.LayoutParams.WRAP_CONTENT));
        TestView shortView = new TestView(context, 12, 20);
        shortView.setLayoutParams(new LayoutParams(12, ViewGroup.LayoutParams.WRAP_CONTENT));
        shortView.setMinimumHeight(30);

        gridRowView.addView(tallView);
        gridRowView.addView(shortView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(new LayoutParams(50, ViewGroup.LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        gridRowView.measure(widthSpec, heightSpec);

        assertThat(tallView.requestedHeight).isEqualTo(50);
        assertThat(shortView.requestedHeight).isEqualTo(30);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowIsTotalOfCells() {
        TestView view1 = new TestView(context, 11, 100);
        view1.setLayoutParams(new LayoutParams(11, 100));
        TestView view2 = new TestView(context, 22, 100);
        view2.setLayoutParams(new LayoutParams(22, 100));

        gridRowView.addView(view1);
        gridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(view1.requestedWidth).isEqualTo(11);
        assertThat(view2.requestedWidth).isEqualTo(22);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(33);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowWithWeightCellsTakesAllSpace() {
        gridRowView.addView(widthView);
        gridRowView.addView(weightView);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(widthView.requestedWidth).isEqualTo(WIDTH);
        assertThat(weightView.requestedWidth).isEqualTo(400 - WIDTH);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowWithMargins() {
        TestView view1 = new TestView(context, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(context, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        gridRowView.addView(goneView);

        gridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(view1.requestedWidth).isEqualTo(11);
        assertThat(view2.requestedWidth).isEqualTo(22);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(33 + 14);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowIsTotalOfCells_collapsibleAndMargins() {
        TestView view1 = new TestView(context, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(context, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        TestView view3 = new TestView(context, 33, 100);
        LayoutParams view3Params = new LayoutParams(LayoutParams.WRAP_CONTENT, 100, 0.0f, true);
        view3Params.leftMargin = 6;
        view3Params.rightMargin = 7;
        view3.setLayoutParams(view3Params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);
        gridRowView.addView(view3);
        gridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(view1.requestedWidth).isEqualTo(11);
        assertThat(view2.requestedWidth).isEqualTo(22);
        assertThat(view3.requestedWidth).isEqualTo(33);
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(11 + 22 + 33 + 2 + 3 + 4 + 5 + 6 + 7);
    }

    @Test
    public void testOnMeasure_truncatesCellsWithMarginsAndPadding() {
        // Cells will fit in the space, but margins push the second cell outside the row

        TestView view1 = new TestView(context, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(context, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);

        gridRowView.setPadding(1, 0, 1, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(33, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(view1.requestedWidth).isEqualTo(11);
        assertThat(view2.requestedWidth).isEqualTo(33 - 11 - 1 - 1 - 2 - 3 - 4); // 11
        assertThat(gridRowView.getMeasuredWidth()).isEqualTo(33);
    }

    @Test
    public void testCalculatesTotalWidth_noWeightCells() {
        TestView view1 = new TestView(context, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(context, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);

        gridRowView.setPadding(6, 0, 7, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(gridRowView.totalContentWidth).isEqualTo(11 + 2 + 3 + 22 + 4 + 5 + 6 + 7);
    }

    @Test
    public void testCalculatesTotalWidth_weightCells() {
        TestView view1 = new TestView(context, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(context, 22, 100);
        LayoutParams view2Params = new LayoutParams(0, 100, 1);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        gridRowView.addView(view1);
        gridRowView.addView(view2);

        gridRowView.setPadding(6, 0, 7, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        gridRowView.measure(widthSpec, unspecifiedSpec);

        assertThat(gridRowView.totalContentWidth).isEqualTo(100);
    }

    private static class TestView extends View {
        private final int desiredWidth;
        private final int desiredHeight;

        int requestedWidth = -3;
        int requestedHeight = -3;

        TestView(Context context, int desiredWidth, int desiredHeight) {
            super(context);
            this.desiredWidth = desiredWidth;
            this.desiredHeight = desiredHeight;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                requestedWidth = MeasureSpec.getSize(widthMeasureSpec);
            } else {
                requestedWidth = desiredWidth;
            }
            if (MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                requestedHeight = MeasureSpec.getSize(heightMeasureSpec);
            } else {
                requestedHeight = desiredHeight;
            }

            int measuredWidth;
            int measuredHeight;
            switch (MeasureSpec.getMode(widthMeasureSpec)) {
                case MeasureSpec.UNSPECIFIED:
                    measuredWidth = desiredWidth;
                    break;
                case MeasureSpec.EXACTLY:
                    measuredWidth = requestedWidth;
                    break;
                case MeasureSpec.AT_MOST:
                    measuredWidth = Math.min(requestedWidth, desiredWidth);
                    break;
                default:
                    measuredWidth = 999999;
            }
            switch (MeasureSpec.getMode(heightMeasureSpec)) {
                case MeasureSpec.UNSPECIFIED:
                    measuredHeight = desiredHeight;
                    break;
                case MeasureSpec.EXACTLY:
                    measuredHeight = requestedHeight;
                    break;
                case MeasureSpec.AT_MOST:
                    measuredHeight = Math.min(requestedHeight, desiredHeight);
                    break;
                default:
                    measuredHeight = 999999;
            }

            setMeasuredDimension(measuredWidth, measuredHeight);
        }
    }

    private static class SquareView extends View {
        SquareView(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width;
            if (MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED) {
                width = 999999;
            } else {
                width = MeasureSpec.getSize(widthMeasureSpec);
            }
            setMeasuredDimension(width, width);
        }
    }
}

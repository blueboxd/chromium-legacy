// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.piet.ui.GridRowView.LayoutParams;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link GridRowView}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GridRowViewTest {
    private static final int WIDTH = 100;
    private static final int WRAP_CONTENT_WIDTH = 75;
    private static final int COLLAPSIBLE_WIDTH = 80;
    private static final int HEIGHT = 1234;
    private static final int LARGE_HEIGHT = 10000;

    private int mUnspecifiedSpec;

    private Context mContext;

    private TestView mCollapsibleView;
    private TestView mCollapsibleView2;
    private TestView mWrapContentView;
    private TestView mWidthView;
    private TestView mWeightView;
    private TestView mWeightView2;
    private TestView mGoneView;

    private GridRowView mGridRowView;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();

        mUnspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mGridRowView = new GridRowView(mContext, Suppliers.of(false));

        mCollapsibleView = new TestView(mContext, COLLAPSIBLE_WIDTH, HEIGHT);
        mCollapsibleView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, true));

        mCollapsibleView2 = new TestView(mContext, COLLAPSIBLE_WIDTH, HEIGHT);
        mCollapsibleView2.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, true));

        mWrapContentView = new TestView(mContext, WRAP_CONTENT_WIDTH, HEIGHT);
        mWrapContentView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, HEIGHT, 0.0f, false));
        // Not sure how to get the wrap content width

        mWidthView = new TestView(mContext, WIDTH, HEIGHT);
        mWidthView.setLayoutParams(new LayoutParams(WIDTH, HEIGHT, 0.0f, false));

        mWeightView = new TestView(mContext, 0, HEIGHT);
        mWeightView.setLayoutParams(new LayoutParams(0, HEIGHT, 1.0f, false));

        mWeightView2 = new TestView(mContext, 0, HEIGHT);
        mWeightView2.setLayoutParams(new LayoutParams(0, HEIGHT, 1.0f, false));

        mGoneView = new TestView(mContext, WIDTH, LARGE_HEIGHT);
        mGoneView.setMinimumHeight(LARGE_HEIGHT);
        mGoneView.setMinimumWidth(WIDTH);
        mGoneView.setVisibility(View.GONE);
    }

    @Test
    public void testOnlyHorizontal() {
        assertThat(mGridRowView.getOrientation()).isEqualTo(LinearLayout.HORIZONTAL);
        assertThatRunnable(() -> mGridRowView.setOrientation(LinearLayout.VERTICAL))
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
        LayoutParams layoutParams = mGridRowView.generateDefaultLayoutParams();
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
        result = mGridRowView.generateLayoutParams(viewGroupLayoutParams);
        assertThat(result.width).isEqualTo(12);
        assertThat(result.height).isEqualTo(34);

        // LinearLayout
        LinearLayout.LayoutParams linearLayoutParams = new LinearLayout.LayoutParams(12, 34, 56);
        result = mGridRowView.generateLayoutParams(linearLayoutParams);
        assertThat(result.width).isEqualTo(12);
        assertThat(result.height).isEqualTo(34);
        assertThat(result.weight).isEqualTo(56.0f);

        // GridRowView
        GridRowView.LayoutParams gridRowParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, 34, 0, true);
        result = mGridRowView.generateLayoutParams(gridRowParams);
        assertThat(result.width).isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(result.height).isEqualTo(34);
        assertThat(result.weight).isEqualTo(0.0f);
        assertThat(result.getIsCollapsible()).isTrue();
    }

    @Test
    public void testCheckLayoutParams() {
        // not LinearLayout
        ViewGroup.LayoutParams viewGroupLayoutParams = new ViewGroup.LayoutParams(12, 34);
        assertThat(mGridRowView.checkLayoutParams(viewGroupLayoutParams)).isFalse();

        // LinearLayout
        LinearLayout.LayoutParams linearLayoutParams = new LinearLayout.LayoutParams(12, 34, 56);
        assertThat(mGridRowView.checkLayoutParams(linearLayoutParams)).isFalse();

        // GridRowView
        GridRowView.LayoutParams gridRowParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, 34, 0, true);
        assertThat(mGridRowView.checkLayoutParams(gridRowParams)).isTrue();
    }

    @Test
    public void testOnMeasure_noCollapsible() {
        // No special processing

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mWeightView);
        mGridRowView.addView(mWeightView2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(100); // WIDTH
        assertThat(mWeightView.mRequestedWidth).isEqualTo(150); // (400 - WIDTH) / 2
        assertThat(mWeightView2.mRequestedWidth).isEqualTo(150);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_unspecifiedWidth() {
        // No special processing; collapsible cell is WRAP_CONTENT

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.measure(mUnspecifiedSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(100); // WIDTH
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(255); // 100 + 80 + 75
    }

    @Test
    public void testOnMeasure_moreSpaceThanNecessary() {
        // DP, Collapsible, WRAP_CONTENT
        // Extra space is left over

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_notEnoughSpace() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is truncated

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(200 - WIDTH - WRAP_CONTENT_WIDTH);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_noSpace() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is completely gone

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(160, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(0);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(160 - WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(160);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForBoth() {
        // Both cells WRAP_CONTENT and there is space left over

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mCollapsibleView2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mCollapsibleView2.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForFirst() {
        // First cell should WRAP_CONTENT, second cell shrinks

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mCollapsibleView2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mCollapsibleView2.mRequestedWidth).isEqualTo(100 - COLLAPSIBLE_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_twoCollapsible_spaceForNeither() {
        // First cell fills row, and second gets no space

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mCollapsibleView2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(50);
        assertThat(mCollapsibleView2.mRequestedWidth).isEqualTo(0);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_collapsibleWithWeight_enoughSpace() {
        // Collapsible cell should WRAP_CONTENT, and weight fills the rest

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWeightView);
        mGridRowView.addView(mWeightView2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mWeightView.mRequestedWidth).isEqualTo((100 - COLLAPSIBLE_WIDTH) / 2);
        assertThat(mWeightView2.mRequestedWidth).isEqualTo((100 - COLLAPSIBLE_WIDTH) / 2);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_collapsibleWithWeight_notEnoughSpace() {
        // Collapsible cell should take up the entire row

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWeightView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(50);
        assertThat(mWeightView.mRequestedWidth).isEqualTo(0);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_widthCollapsible_enoughSpace() {
        // DP, Collapsible, WRAP
        // Extra space is left over

        mCollapsibleView.setLayoutParams(new LayoutParams(COLLAPSIBLE_WIDTH, HEIGHT, 0.0f, true));

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_widthCollapsible_notEnoughSpace() {
        // DP, Collapsible, WRAP
        // Collapsible cell is truncated

        mCollapsibleView.setLayoutParams(new LayoutParams(COLLAPSIBLE_WIDTH, HEIGHT, 0.0f, true));

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(200 - WIDTH - WRAP_CONTENT_WIDTH);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_cellThatGetsTallerWithWidth() {
        // Make a cell where width = height and weight = 1 and ensure it behaves with a collapsible

        mCollapsibleView = new TestView(mContext, COLLAPSIBLE_WIDTH, 20);
        mCollapsibleView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, 20, 0.0f, true));

        SquareView squareView = new SquareView(mContext);
        squareView.setLayoutParams(new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f, false));

        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(squareView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(130, MeasureSpec.EXACTLY);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        int squareSize = 130 - COLLAPSIBLE_WIDTH;
        assertThat(mCollapsibleView.mRequestedWidth).isEqualTo(COLLAPSIBLE_WIDTH);
        assertThat(squareView.getMeasuredWidth()).isEqualTo(squareSize);
        assertThat(squareView.getMeasuredHeight()).isEqualTo(squareSize);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(130);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(squareSize);
    }

    @Test
    public void testOnMeasure_wrapContentMatchesCellHeight() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(40);
    }

    @Test
    public void testOnMeasure_doesNotWrapHeightWhenMeasureExactly() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, 100));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_shrinksCellThatWantsToBeTooTall() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView view = new TestView(mContext, 150, 200);
        view.setLayoutParams(new LayoutParams(150, 200, 0.0f, true));

        mGridRowView.addView(view);

        mGridRowView.setLayoutParams(new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, 100));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(100);
        assertThat(view.mRequestedWidth).isEqualTo(100);
        assertThat(view.mRequestedHeight).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_respectsMinHeight() {
        // Several different sizes of cells; ensure that height of GridRowView is the min height.

        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        mGridRowView.setMinimumHeight(95);

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(95);
    }

    @Test
    public void testOnMeasure_respectsMinHeightUpToAtMost() {
        // Several different sizes of cells; ensure that height of GridRowView is the min height.

        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(new LayoutParams(20, 20, 0.0f, true));
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(new LayoutParams(40, 40, 0.0f, true));
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(new LayoutParams(30, 30, 0.0f, true));

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        mGridRowView.setMinimumHeight(1234);

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_respectsChildLayoutParams_weight() {
        // Several different sizes of cells; ensure that width of each cell is set by the style

        int styleWidth = 30;
        LayoutParams cellLayoutParams =
                new LayoutParams(styleWidth, LayoutParams.WRAP_CONTENT, 0.0f, true);
        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(cellLayoutParams);
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(cellLayoutParams);
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(cellLayoutParams);

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(square1.mRequestedWidth).isEqualTo(styleWidth);
        assertThat(square2.mRequestedWidth).isEqualTo(styleWidth);
        assertThat(square3.mRequestedWidth).isEqualTo(styleWidth);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(200);
    }

    @Test
    public void testOnMeasure_respectsChildLayoutParams_height() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        int styleHeight = 30;
        LayoutParams cellLayoutParams =
                new LayoutParams(LayoutParams.WRAP_CONTENT, styleHeight, 0.0f, true);
        TestView square1 = new TestView(mContext, 20, 20);
        square1.setLayoutParams(cellLayoutParams);
        TestView square2 = new TestView(mContext, 40, 40);
        square2.setLayoutParams(cellLayoutParams);
        TestView square3 = new TestView(mContext, 30, 30);
        square3.setLayoutParams(cellLayoutParams);

        mGridRowView.addView(square1);
        mGridRowView.addView(square2);
        mGridRowView.addView(square3);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(square1.mRequestedHeight).isEqualTo(styleHeight);
        assertThat(square2.mRequestedHeight).isEqualTo(styleHeight);
        assertThat(square3.mRequestedHeight).isEqualTo(styleHeight);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(styleHeight);
    }

    @Test
    public void testOnMeasure_rowHeightIsMaxCellHeightWithMargins() {
        // Several different sizes of cells; ensure that height of GridRowView is same as tallest
        // cell.

        TestView view1 = new TestView(mContext, 40, 40);
        view1.setLayoutParams(new LayoutParams(40, 40));

        TestView view2 = new TestView(mContext, 30, 30);
        LayoutParams params = new LayoutParams(30, 30);
        params.topMargin = 5;
        params.bottomMargin = 15;
        view2.setLayoutParams(params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new ViewGroup.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        int sizeSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(100);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(50);
    }

    @Test
    public void testOnMeasure_cellWithMarginsClippedByHeightConstraint() {
        // Row is 100 high, cell is set to 90 high, but has 5 + 15 margins, so cell is only 80 high.

        TestView view = new TestView(mContext, 123, 90);
        LayoutParams params = new LayoutParams(123, 90);
        params.topMargin = 5;
        params.bottomMargin = 15;
        view.setLayoutParams(params);

        mGridRowView.addView(view);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);

        mGridRowView.measure(widthSpec, heightSpec);

        assertThat(view.mRequestedHeight).isEqualTo(80);
    }

    @Test
    public void testOnMeasure_respectsGridRowPadding() {
        // DP, Collapsible, WRAP_CONTENT
        // Collapsible cell is truncated

        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mCollapsibleView);
        mGridRowView.addView(mWrapContentView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        int paddingLeft = 10;
        int paddingTop = 20;
        int paddingRight = 30;
        int paddingBottom = 40;
        mGridRowView.setPadding(paddingLeft, paddingTop, paddingRight, paddingBottom);

        int sizeSpec = MeasureSpec.makeMeasureSpec(250, MeasureSpec.EXACTLY);
        mGridRowView.measure(sizeSpec, sizeSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mCollapsibleView.mRequestedWidth)
                .isEqualTo(250 - WIDTH - WRAP_CONTENT_WIDTH - paddingLeft - paddingRight);
        assertThat(mWrapContentView.mRequestedWidth).isEqualTo(WRAP_CONTENT_WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(250);

        int cellHeight = 250 - paddingTop - paddingBottom;
        assertThat(mWidthView.mRequestedHeight).isEqualTo(cellHeight);
        assertThat(mCollapsibleView.mRequestedHeight).isEqualTo(cellHeight);
        assertThat(mWrapContentView.mRequestedHeight).isEqualTo(cellHeight);
        assertThat(mGridRowView.getMeasuredHeight()).isEqualTo(250);
    }

    @Test
    public void testOnMeasure_fillParentCellsWithFitContentRow() {
        TestView tallView = new TestView(mContext, 11, 100);
        tallView.setLayoutParams(new LayoutParams(11, ViewGroup.LayoutParams.MATCH_PARENT));
        TestView shortView = new TestView(mContext, 12, 20);
        shortView.setLayoutParams(new LayoutParams(12, ViewGroup.LayoutParams.MATCH_PARENT));

        mGridRowView.addView(tallView);
        mGridRowView.addView(shortView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(new LayoutParams(50, ViewGroup.LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mGridRowView.measure(widthSpec, heightSpec);

        assertThat(tallView.mRequestedHeight).isEqualTo(100);
        assertThat(shortView.mRequestedHeight).isEqualTo(100);
    }

    @Test
    public void testOnMeasure_fitContentCellsWithFitContentRow_minHeight() {
        TestView tallView = new TestView(mContext, 11, 50);
        tallView.setLayoutParams(new LayoutParams(11, ViewGroup.LayoutParams.WRAP_CONTENT));
        TestView shortView = new TestView(mContext, 12, 20);
        shortView.setLayoutParams(new LayoutParams(12, ViewGroup.LayoutParams.WRAP_CONTENT));
        shortView.setMinimumHeight(30);

        mGridRowView.addView(tallView);
        mGridRowView.addView(shortView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(new LayoutParams(50, ViewGroup.LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        mGridRowView.measure(widthSpec, heightSpec);

        assertThat(tallView.mRequestedHeight).isEqualTo(50);
        assertThat(shortView.mRequestedHeight).isEqualTo(30);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowIsTotalOfCells() {
        TestView view1 = new TestView(mContext, 11, 100);
        view1.setLayoutParams(new LayoutParams(11, 100));
        TestView view2 = new TestView(mContext, 22, 100);
        view2.setLayoutParams(new LayoutParams(22, 100));

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(view1.mRequestedWidth).isEqualTo(11);
        assertThat(view2.mRequestedWidth).isEqualTo(22);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(33);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowWithWeightCellsTakesAllSpace() {
        mGridRowView.addView(mWidthView);
        mGridRowView.addView(mWeightView);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mWidthView.mRequestedWidth).isEqualTo(WIDTH);
        assertThat(mWeightView.mRequestedWidth).isEqualTo(400 - WIDTH);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(400);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowWithMargins() {
        TestView view1 = new TestView(mContext, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(mContext, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);
        // Verify views which are gone are not considered in the layout.
        mGridRowView.addView(mGoneView);

        mGridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(view1.mRequestedWidth).isEqualTo(11);
        assertThat(view2.mRequestedWidth).isEqualTo(22);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(33 + 14);
    }

    @Test
    public void testOnMeasure_fitContentWidthRowIsTotalOfCells_collapsibleAndMargins() {
        TestView view1 = new TestView(mContext, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(mContext, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        TestView view3 = new TestView(mContext, 33, 100);
        LayoutParams view3Params = new LayoutParams(LayoutParams.WRAP_CONTENT, 100, 0.0f, true);
        view3Params.leftMargin = 6;
        view3Params.rightMargin = 7;
        view3.setLayoutParams(view3Params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);
        mGridRowView.addView(view3);
        mGridRowView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        int widthSpec = MeasureSpec.makeMeasureSpec(400, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(view1.mRequestedWidth).isEqualTo(11);
        assertThat(view2.mRequestedWidth).isEqualTo(22);
        assertThat(view3.mRequestedWidth).isEqualTo(33);
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(11 + 22 + 33 + 2 + 3 + 4 + 5 + 6 + 7);
    }

    @Test
    public void testOnMeasure_truncatesCellsWithMarginsAndPadding() {
        // Cells will fit in the space, but margins push the second cell outside the row

        TestView view1 = new TestView(mContext, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(mContext, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);

        mGridRowView.setPadding(1, 0, 1, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(33, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(view1.mRequestedWidth).isEqualTo(11);
        assertThat(view2.mRequestedWidth).isEqualTo(33 - 11 - 1 - 1 - 2 - 3 - 4); // 11
        assertThat(mGridRowView.getMeasuredWidth()).isEqualTo(33);
    }

    @Test
    public void testCalculatesTotalWidth_noWeightCells() {
        TestView view1 = new TestView(mContext, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(mContext, 22, 100);
        LayoutParams view2Params = new LayoutParams(22, 100);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);

        mGridRowView.setPadding(6, 0, 7, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mGridRowView.mTotalContentWidth).isEqualTo(11 + 2 + 3 + 22 + 4 + 5 + 6 + 7);
    }

    @Test
    public void testCalculatesTotalWidth_weightCells() {
        TestView view1 = new TestView(mContext, 11, 100);
        LayoutParams view1Params = new LayoutParams(11, 100);
        view1Params.leftMargin = 2;
        view1Params.rightMargin = 3;
        view1.setLayoutParams(view1Params);

        TestView view2 = new TestView(mContext, 22, 100);
        LayoutParams view2Params = new LayoutParams(0, 100, 1);
        view2Params.leftMargin = 4;
        view2Params.rightMargin = 5;
        view2.setLayoutParams(view2Params);

        mGridRowView.addView(view1);
        mGridRowView.addView(view2);

        mGridRowView.setPadding(6, 0, 7, 0);

        int widthSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.AT_MOST);
        mGridRowView.measure(widthSpec, mUnspecifiedSpec);

        assertThat(mGridRowView.mTotalContentWidth).isEqualTo(100);
    }

    private static class TestView extends View {
        private final int mDesiredWidth;
        private final int mDesiredHeight;

        int mRequestedWidth = -3;
        int mRequestedHeight = -3;

        TestView(Context context, int desiredWidth, int desiredHeight) {
            super(context);
            this.mDesiredWidth = desiredWidth;
            this.mDesiredHeight = desiredHeight;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                mRequestedWidth = MeasureSpec.getSize(widthMeasureSpec);
            } else {
                mRequestedWidth = mDesiredWidth;
            }
            if (MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.UNSPECIFIED) {
                mRequestedHeight = MeasureSpec.getSize(heightMeasureSpec);
            } else {
                mRequestedHeight = mDesiredHeight;
            }

            int measuredWidth;
            int measuredHeight;
            switch (MeasureSpec.getMode(widthMeasureSpec)) {
                case MeasureSpec.UNSPECIFIED:
                    measuredWidth = mDesiredWidth;
                    break;
                case MeasureSpec.EXACTLY:
                    measuredWidth = mRequestedWidth;
                    break;
                case MeasureSpec.AT_MOST:
                    measuredWidth = Math.min(mRequestedWidth, mDesiredWidth);
                    break;
                default:
                    measuredWidth = 999999;
            }
            switch (MeasureSpec.getMode(heightMeasureSpec)) {
                case MeasureSpec.UNSPECIFIED:
                    measuredHeight = mDesiredHeight;
                    break;
                case MeasureSpec.EXACTLY:
                    measuredHeight = mRequestedHeight;
                    break;
                case MeasureSpec.AT_MOST:
                    measuredHeight = Math.min(mRequestedHeight, mDesiredHeight);
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.os.Build.VERSION_CODES;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.ui.BorderDrawable;
import com.google.android.libraries.feed.piet.ui.GradientDrawable;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.android.libraries.feed.piet.ui.RoundedCornerWrapperView;
import com.google.search.now.ui.piet.GradientsProto.ColorStop;
import com.google.search.now.ui.piet.GradientsProto.Fill;
import com.google.search.now.ui.piet.GradientsProto.LinearGradient;
import com.google.search.now.ui.piet.ImagesProto.Image.ScaleType;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.ShadowsProto.ElevationShadow;
import com.google.search.now.ui.piet.ShadowsProto.Shadow;
import com.google.search.now.ui.piet.StylesProto.Borders;
import com.google.search.now.ui.piet.StylesProto.Borders.Edges;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.GravityHorizontal;
import com.google.search.now.ui.piet.StylesProto.GravityVertical;
import com.google.search.now.ui.piet.StylesProto.ImageLoadingSettings;
import com.google.search.now.ui.piet.StylesProto.RelativeSize;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.TextAlignmentHorizontal;
import com.google.search.now.ui.piet.StylesProto.TextAlignmentVertical;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** TODO: Test remaining methods of StyleProvider. */
@RunWith(RobolectricTestRunner.class)
public class StyleProviderTest {
    @Mock
    private AssetProvider mockAssetProvider;
    @Mock
    private ElementAdapter<View, ?> adapter;
    @Mock
    private TextElementAdapter textAdapter;
    @Mock
    private RoundedCornerMaskCache maskCache;

    private View view;
    private View baseView;
    private TextView textView;

    private Context context;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        view = new View(context);
        baseView = new View(context);
        textView = new TextView(context);
        when(adapter.getView()).thenReturn(view);
        when(adapter.getBaseView()).thenReturn(baseView);
        when(adapter.getContext()).thenReturn(context);
        when(textAdapter.getView()).thenReturn(view);
        when(textAdapter.getBaseView()).thenReturn(textView);
        when(textAdapter.getContext()).thenReturn(context);
        when(mockAssetProvider.isRtLSupplier()).thenReturn(Suppliers.of(false));
    }

    @Test
    public void testGetters_withStyleDefined() {
        Style style =
                Style.newBuilder()
                        .setColor(1)
                        .setBackground(Fill.newBuilder().setColor(2))
                        .setImageLoadingSettings(
                                ImageLoadingSettings.newBuilder()
                                        .setPreLoadFill(Fill.newBuilder().setColor(9))
                                        .setFadeInImageOnLoad(true)
                                        .build())
                        .setRoundedCorners(RoundedCorners.newBuilder().setRadiusDp(3))
                        .setFont(Font.newBuilder().setSize(4))
                        .setMaxLines(5)
                        .setMinHeight(6)
                        .setHeight(7)
                        .setWidth(8)
                        .setScaleType(ScaleType.CENTER_CROP)
                        .setGravityHorizontal(GravityHorizontal.GRAVITY_END)
                        .setGravityVertical(GravityVertical.GRAVITY_BOTTOM)
                        .setTextAlignmentHorizontal(TextAlignmentHorizontal.TEXT_ALIGNMENT_CENTER)
                        .setTextAlignmentVertical(TextAlignmentVertical.TEXT_ALIGNMENT_BOTTOM)
                        .build();

        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);

        assertThat(styleProvider.getColor()).isEqualTo(style.getColor());
        assertThat(styleProvider.getBackground()).isEqualTo(style.getBackground());
        assertThat(styleProvider.hasPreLoadFill()).isTrue();
        assertThat(styleProvider.getPreLoadFill())
                .isEqualTo(style.getImageLoadingSettings().getPreLoadFill());
        assertThat(styleProvider.getFadeInImageOnLoad())
                .isEqualTo(style.getImageLoadingSettings().getFadeInImageOnLoad());
        assertThat(styleProvider.getRoundedCorners()).isEqualTo(style.getRoundedCorners());
        assertThat(styleProvider.hasRoundedCorners()).isTrue();
        assertThat(styleProvider.getFont()).isEqualTo(style.getFont());
        assertThat(styleProvider.getMaxLines()).isEqualTo(style.getMaxLines());
        assertThat(styleProvider.getHeightSpecPx(context))
                .isEqualTo((int) LayoutUtils.dpToPx(style.getHeight(), context));
        assertThat(styleProvider.getWidthSpecPx(context))
                .isEqualTo((int) LayoutUtils.dpToPx(style.getWidth(), context));
        assertThat(styleProvider.hasHeight()).isTrue();
        assertThat(styleProvider.hasWidth()).isTrue();
        assertThat(styleProvider.getScaleType()).isEqualTo(ImageView.ScaleType.CENTER_CROP);
        assertThat(styleProvider.hasGravityHorizontal()).isTrue();
        assertThat(styleProvider.getGravityHorizontal(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.END);
        assertThat(styleProvider.hasGravityVertical()).isTrue();
        assertThat(styleProvider.getGravityVertical(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.BOTTOM);
        assertThat(styleProvider.getGravity(Gravity.FILL)).isEqualTo(Gravity.END | Gravity.BOTTOM);
        assertThat(styleProvider.getTextAlignment())
                .isEqualTo(Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM);
    }

    @Test
    public void testGetters_withEmptyStyleDefined() {
        Style style = Style.getDefaultInstance();

        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);

        assertThat(styleProvider.getColor()).isEqualTo(style.getColor());
        assertThat(styleProvider.getBackground()).isEqualTo(style.getBackground());
        assertThat(styleProvider.hasPreLoadFill()).isFalse();
        assertThat(styleProvider.getPreLoadFill()).isEqualTo(Fill.getDefaultInstance());
        assertThat(styleProvider.getFadeInImageOnLoad()).isFalse();
        assertThat(styleProvider.hasRoundedCorners()).isFalse();
        assertThat(styleProvider.getRoundedCorners())
                .isEqualTo(RoundedCorners.getDefaultInstance());
        assertThat(styleProvider.getFont()).isEqualTo(Font.getDefaultInstance());
        assertThat(styleProvider.getMaxLines()).isEqualTo(style.getMaxLines());
        assertThat(styleProvider.getHeightSpecPx(context)).isEqualTo(DIMENSION_NOT_SET);
        assertThat(styleProvider.getWidthSpecPx(context)).isEqualTo(DIMENSION_NOT_SET);
        assertThat(styleProvider.hasHeight()).isFalse();
        assertThat(styleProvider.hasWidth()).isFalse();
        assertThat(styleProvider.getScaleType()).isEqualTo(ImageView.ScaleType.FIT_CENTER);
        assertThat(styleProvider.hasGravityHorizontal()).isFalse();
        assertThat(styleProvider.getGravityHorizontal(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CLIP_VERTICAL);
        assertThat(styleProvider.hasGravityVertical()).isFalse();
        assertThat(styleProvider.getGravityVertical(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CLIP_HORIZONTAL);
        assertThat(styleProvider.getGravity(Gravity.FILL)).isEqualTo(Gravity.FILL);
        assertThat(styleProvider.getTextAlignment()).isEqualTo(Gravity.START | Gravity.TOP);
    }

    @Test
    public void testNoOverlapWithAndroidConstants() {
        assertThat(DIMENSION_NOT_SET).isNotEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(DIMENSION_NOT_SET).isNotEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(DIMENSION_NOT_SET).isLessThan(0);
    }

    @Test
    public void testGetHeightSpecPx() {
        assertThat(new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                           .getHeightSpecPx(context))
                .isEqualTo(DIMENSION_NOT_SET);

        assertThat(new StyleProvider(Style.newBuilder().setHeight(123).build(), mockAssetProvider)
                           .getHeightSpecPx(context))
                .isEqualTo(123);

        assertThat(new StyleProvider(
                           Style.newBuilder().setRelativeHeight(RelativeSize.FILL_PARENT).build(),
                           mockAssetProvider)
                           .getHeightSpecPx(context))
                .isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(new StyleProvider(
                           Style.newBuilder().setRelativeHeight(RelativeSize.FIT_CONTENT).build(),
                           mockAssetProvider)
                           .getHeightSpecPx(context))
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                new StyleProvider(Style.newBuilder()
                                          .setRelativeHeight(RelativeSize.RELATIVE_SIZE_UNDEFINED)
                                          .build(),
                        mockAssetProvider)
                        .getHeightSpecPx(context))
                .isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testGetWidthSpecPx() {
        assertThat(new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                           .getWidthSpecPx(context))
                .isEqualTo(DIMENSION_NOT_SET);

        assertThat(new StyleProvider(Style.newBuilder().setWidth(123).build(), mockAssetProvider)
                           .getWidthSpecPx(context))
                .isEqualTo(123);

        assertThat(new StyleProvider(
                           Style.newBuilder().setRelativeWidth(RelativeSize.FILL_PARENT).build(),
                           mockAssetProvider)
                           .getWidthSpecPx(context))
                .isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(new StyleProvider(
                           Style.newBuilder().setRelativeWidth(RelativeSize.FIT_CONTENT).build(),
                           mockAssetProvider)
                           .getWidthSpecPx(context))
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(new StyleProvider(Style.newBuilder()
                                             .setRelativeWidth(RelativeSize.RELATIVE_SIZE_UNDEFINED)
                                             .build(),
                           mockAssetProvider)
                           .getWidthSpecPx(context))
                .isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testHasBorders_falseIfWidthZero() {
        StyleProvider styleProvider = new StyleProvider(
                Style.newBuilder().setBorders(Borders.newBuilder().setBitmask(15)).build(),
                mockAssetProvider);

        assertThat(styleProvider.hasBorders()).isFalse();
    }

    @Test
    public void testHasRoundedCornersMethod_noRoundedCorners() {
        Style noRoundedCornerStyle = Style.getDefaultInstance();
        StyleProvider styleProvider = new StyleProvider(noRoundedCornerStyle, mockAssetProvider);
        when(mockAssetProvider.getDefaultCornerRadius()).thenReturn(321);

        assertThat(styleProvider.hasRoundedCorners()).isFalse();
    }

    @Test
    public void testApplyPadding() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                .applyPadding(context, view, padding,
                        TextElementAdapter.ExtraLineHeight.builder().build());
        verifyPadding(view, padding);
    }

    @Test
    public void testApplyPadding_RtL() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        when(mockAssetProvider.isRtL()).thenReturn(true);
        when(mockAssetProvider.isRtLSupplier()).thenReturn(Suppliers.of(true));
        new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                .applyPadding(context, view, padding,
                        TextElementAdapter.ExtraLineHeight.builder().build());
        assertThat(view.getPaddingLeft()).isEqualTo(4);
        assertThat(view.getPaddingRight()).isEqualTo(3);
    }

    @Config(sdk = VERSION_CODES.JELLY_BEAN)
    @Test
    public void testApplyPadding_JB() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        new StyleProvider(Style.newBuilder().setPadding(padding).build(), mockAssetProvider)
                .applyElementStyles(adapter);
        verifyPadding(baseView, padding);
    }

    @Test
    public void testApplyPadding_lineHeight() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        TextElementAdapter.ExtraLineHeight extraLineHeightPadding =
                TextElementAdapter.ExtraLineHeight.builder()
                        .setTopPaddingPx(50)
                        .setBottomPaddingPx(60)
                        .build();
        Style paddingStyle = Style.newBuilder().setPadding(padding).build();
        StyleProvider styleProvider = new StyleProvider(paddingStyle, mockAssetProvider);

        styleProvider.applyPadding(context, view, padding, extraLineHeightPadding);

        // line height top padding (50) + regular top padding (1) = expected final top (51)
        assertThat(view.getPaddingTop()).isEqualTo(51);
        // line height bottom padding (60) + regular bottom padding (2) = expected final bottom (62)
        assertThat(view.getPaddingBottom()).isEqualTo(62);
    }

    @Test
    public void testApplyPadding_addsExtraForBorders_allSides() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        Style bordersStyle =
                Style.newBuilder().setBorders(Borders.newBuilder().setWidth(5)).build();
        new StyleProvider(bordersStyle, mockAssetProvider)
                .applyPadding(context, view, padding,
                        TextElementAdapter.ExtraLineHeight.builder().build());

        assertThat(view.getPaddingTop()).isEqualTo((int) LayoutUtils.dpToPx(1 + 5, context));
        assertThat(view.getPaddingBottom()).isEqualTo((int) LayoutUtils.dpToPx(2 + 5, context));
        assertThat(view.getPaddingStart()).isEqualTo((int) LayoutUtils.dpToPx(3 + 5, context));
        assertThat(view.getPaddingEnd()).isEqualTo((int) LayoutUtils.dpToPx(4 + 5, context));
    }

    @Test
    public void testApplyPadding_addsExtraForBorders_someSides() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        Style bordersBottomRight =
                Style.newBuilder()
                        .setBorders(Borders.newBuilder().setWidth(5).setBitmask(
                                Edges.BOTTOM.getNumber() | Edges.END.getNumber()))
                        .build();
        new StyleProvider(bordersBottomRight, mockAssetProvider)
                .applyPadding(context, view, padding,
                        TextElementAdapter.ExtraLineHeight.builder().build());

        assertThat(view.getPaddingTop()).isEqualTo((int) LayoutUtils.dpToPx(1, context));
        assertThat(view.getPaddingBottom()).isEqualTo((int) LayoutUtils.dpToPx(2 + 5, context));
        assertThat(view.getPaddingStart()).isEqualTo((int) LayoutUtils.dpToPx(3, context));
        assertThat(view.getPaddingEnd()).isEqualTo((int) LayoutUtils.dpToPx(4 + 5, context));
    }

    @Test
    public void testCreateBorderDrawable() {
        Borders borders = Borders.newBuilder()
                                  .setColor(Color.CYAN)
                                  .setWidth(4)
                                  .setBitmask(Edges.TOP.getNumber() | Edges.END.getNumber())
                                  .build();

        FrameLayout view = new FrameLayout(context);
        new StyleProvider(Style.newBuilder().setBorders(borders).build(), mockAssetProvider)
                .addBordersWithoutRoundedCorners(view, context);
        BorderDrawable borderDrawable = (BorderDrawable) view.getForeground();
        borderDrawable.setBounds(0, 0, 0, 0);

        assertThat(borderDrawable.getPaint().getColor()).isEqualTo(Color.CYAN);
        assertThat(borderDrawable.getBounds()).isEqualTo(new Rect(-4, 0, 0, 4));
    }

    @Test
    public void testCreateBorderDrawable_noOp() {
        FrameLayout view = new FrameLayout(context);
        new StyleProvider(Style.newBuilder().setBorders(Borders.newBuilder().setWidth(0)).build(),
                mockAssetProvider)
                .addBordersWithoutRoundedCorners(view, context);

        assertThat(view.getForeground()).isNull();
    }

    @Test
    public void testCreateWrapperView_noRoundedCorners() {
        StyleProvider styleProvider = new StyleProvider(
                Style.newBuilder()
                        .setRoundedCorners(RoundedCorners.newBuilder().setRadiusDp(0).setBitmask(4))
                        .build(),
                mockAssetProvider);

        FrameLayout wrapperView = styleProvider.createWrapperView(context, maskCache, false, false);

        assertThat(wrapperView).isNotInstanceOf(RoundedCornerWrapperView.class);
        assertThat(wrapperView.getOutlineProvider()).isEqualTo(ViewOutlineProvider.BOUNDS);
    }

    @Test
    public void testCreateWrapperView_roundedCorners() {
        StyleProvider styleProvider = new StyleProvider(
                Style.newBuilder()
                        .setRoundedCorners(
                                RoundedCorners.newBuilder().setRadiusDp(16).setBitmask(4))
                        .build(),
                mockAssetProvider);

        FrameLayout wrapperView = styleProvider.createWrapperView(context, maskCache, false, false);

        assertThat(wrapperView).isInstanceOf(RoundedCornerWrapperView.class);
        assertThat(wrapperView.getOutlineProvider()).isNotEqualTo(ViewOutlineProvider.BOUNDS);
    }

    @Test
    public void testApplyMargins() {
        EdgeWidths margins =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        Style marginStyle = Style.newBuilder().setMargins(margins).build();

        MarginLayoutParams params = new MarginLayoutParams(
                MarginLayoutParams.MATCH_PARENT, MarginLayoutParams.WRAP_CONTENT);

        new StyleProvider(marginStyle, mockAssetProvider).applyMargins(context, params);
        verifyMargins(params, margins);
    }

    @Test
    public void testElementStyles_padding() {
        EdgeWidths padding =
                EdgeWidths.newBuilder().setTop(1).setBottom(2).setStart(3).setEnd(4).build();
        Style paddingStyle = Style.newBuilder().setPadding(padding).build();

        new StyleProvider(paddingStyle, mockAssetProvider).applyElementStyles(adapter);
        verifyPadding(baseView, padding);
    }

    @Test
    public void testElementStyles_backgroundAndCorners() {
        int color = 0xffffffff;
        Fill background = Fill.newBuilder().setColor(color).build();
        RoundedCorners corners = RoundedCorners.newBuilder().setRadiusDp(4).setBitmask(3).build();

        Style style = Style.newBuilder()
                              .setColor(color)
                              .setBackground(background)
                              .setRoundedCorners(corners)
                              .build();

        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        styleProvider.applyElementStyles(adapter);

        ColorDrawable colorDrawable = (ColorDrawable) baseView.getBackground();
        assertThat(colorDrawable.getColor()).isEqualTo(color);
    }

    @Test
    public void testElementStyles_noBackgroundInStyle() {
        baseView.setBackground(new ColorDrawable(0xffff0000));

        new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                .applyElementStyles(adapter);
        assertThat(baseView.getBackground()).isNull();
    }

    @Test
    public void testElementStyles_noColorInFill() {
        baseView.setBackground(new ColorDrawable(0xffff0000));

        Style style = Style.newBuilder().setBackground(Fill.getDefaultInstance()).build();

        new StyleProvider(style, mockAssetProvider).applyElementStyles(adapter);

        assertThat(baseView.getBackground()).isNull();
    }

    @Test
    public void testElementStyles_gradientBackground() {
        baseView.setBackground(new ColorDrawable(0xffff0000));

        Style style = Style.newBuilder()
                              .setBackground(Fill.newBuilder().setLinearGradient(
                                      LinearGradient.newBuilder()
                                              .setDirectionDeg(25)
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.RED)
                                                                .setPositionPercent(0))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.WHITE)
                                                                .setPositionPercent(25))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.GREEN)
                                                                .setPositionPercent(50))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.WHITE)
                                                                .setPositionPercent(75))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.BLUE)
                                                                .setPositionPercent(100))))
                              .build();

        new StyleProvider(style, mockAssetProvider).applyElementStyles(adapter);

        Drawable background = baseView.getBackground();
        assertThat(background).isInstanceOf(GradientDrawable.class);
    }

    @Test
    public void testElementStyles_gradientBackground_badAngle_doesntCrash() {
        baseView.setBackground(new ColorDrawable(0xffff0000));

        Style style = Style.newBuilder()
                              .setBackground(Fill.newBuilder().setLinearGradient(
                                      LinearGradient.newBuilder()
                                              .setDirectionDeg(999)
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.RED)
                                                                .setPositionPercent(0))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.BLUE)
                                                                .setPositionPercent(100))))
                              .build();

        new StyleProvider(style, mockAssetProvider).applyElementStyles(adapter);

        Drawable background = baseView.getBackground();
        assertThat(background).isInstanceOf(GradientDrawable.class);
    }

    @Test
    public void testElementStyles_gradientBackground_badPercent_doesntCrash() {
        baseView.setBackground(new ColorDrawable(0xffff0000));

        Style style = Style.newBuilder()
                              .setBackground(Fill.newBuilder().setLinearGradient(
                                      LinearGradient.newBuilder()
                                              .setDirectionDeg(45)
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.RED)
                                                                .setPositionPercent(200))
                                              .addStops(ColorStop.newBuilder()
                                                                .setColor(Color.BLUE)
                                                                .setPositionPercent(25))))
                              .build();

        new StyleProvider(style, mockAssetProvider).applyElementStyles(adapter);

        Drawable background = baseView.getBackground();
        assertThat(background).isInstanceOf(GradientDrawable.class);
    }

    @Test
    public void testElementStyles_minimumHeight() {
        int minHeight = 12345;
        Style heightStyle = Style.newBuilder().setMinHeight(minHeight).build();

        new StyleProvider(heightStyle, mockAssetProvider).applyElementStyles(adapter);

        assertThat(baseView.getMinimumHeight()).isEqualTo(minHeight);
    }

    @Test
    public void testElementStyles_noMinimumHeight() {
        Style noHeightStyle = Style.getDefaultInstance();
        view.setMinimumHeight(12345);

        new StyleProvider(noHeightStyle, mockAssetProvider).applyElementStyles(adapter);

        assertThat(baseView.getMinimumHeight()).isEqualTo(0);
    }

    @Test
    public void testElementStyles_shadow() {
        int elevation = 5280;
        Style shadowStyle = Style.newBuilder()
                                    .setShadow(Shadow.newBuilder().setElevationShadow(
                                            ElevationShadow.newBuilder().setElevation(elevation)))
                                    .build();

        new StyleProvider(shadowStyle, mockAssetProvider).applyElementStyles(adapter);

        assertThat(view.getElevation()).isEqualTo((float) elevation);
    }

    @Test
    public void testElementStyles_noShadow() {
        Style shadowStyle = Style.newBuilder()
                                    .setShadow(Shadow.newBuilder().setElevationShadow(
                                            ElevationShadow.newBuilder().setElevation(5280)))
                                    .build();
        Style noShadowStyle = Style.getDefaultInstance();

        new StyleProvider(shadowStyle, mockAssetProvider).applyElementStyles(adapter);

        new StyleProvider(noShadowStyle, mockAssetProvider).applyElementStyles(adapter);

        assertThat(view.getElevation()).isEqualTo(0.0f);
    }

    @Test
    @Config(sdk = VERSION_CODES.JELLY_BEAN)
    public void testElementStyles_shadowDoesNotError_JB() {
        Style shadowStyle = Style.newBuilder()
                                    .setShadow(Shadow.newBuilder().setElevationShadow(
                                            ElevationShadow.newBuilder().setElevation(5280)))
                                    .build();
        Style noShadowStyle = Style.getDefaultInstance();

        new StyleProvider(shadowStyle, mockAssetProvider).applyElementStyles(adapter);

        new StyleProvider(noShadowStyle, mockAssetProvider).applyElementStyles(adapter);

        // Assert nothing failed.
    }

    @Test
    @Config(sdk = VERSION_CODES.KITKAT)
    public void testElementStyles_shadowDoesNotError_KK() {
        Style shadowStyle = Style.newBuilder()
                                    .setShadow(Shadow.newBuilder().setElevationShadow(
                                            ElevationShadow.newBuilder().setElevation(5280)))
                                    .build();
        Style noShadowStyle = Style.getDefaultInstance();

        new StyleProvider(shadowStyle, mockAssetProvider).applyElementStyles(adapter);

        new StyleProvider(noShadowStyle, mockAssetProvider).applyElementStyles(adapter);

        // Assert nothing failed.
    }

    @Test
    public void testElementStyles_opacity() {
        new StyleProvider(Style.newBuilder().setOpacity(0.5f).build(), mockAssetProvider)
                .applyElementStyles(adapter);

        assertThat(baseView.getAlpha()).isEqualTo(0.5f);

        new StyleProvider(Style.getDefaultInstance(), mockAssetProvider)
                .applyElementStyles(adapter);

        assertThat(baseView.getAlpha()).isEqualTo(1.0f);
    }

    @Test
    public void testElementStyles_overlayResets() {
        baseView.setBackground(new ShapeDrawable());
        baseView.setElevation(12.3f);

        new StyleProvider(Style.newBuilder()
                                  .setBackground(Fill.newBuilder().setColor(Color.BLUE))
                                  .setShadow(Shadow.newBuilder().setElevationShadow(
                                          ElevationShadow.newBuilder().setElevation(5)))
                                  .build(),
                mockAssetProvider)
                .applyElementStyles(adapter);

        assertThat(baseView.getElevation()).isWithin(0.1f).of(0.0f);
        assertThat(view.getElevation()).isWithin(0.1f).of(5.0f);

        assertThat(view.getBackground()).isNull();
        assertThat(baseView.getBackground()).isInstanceOf(ColorDrawable.class);
    }

    @Test
    public void testCreateBackground() {
        int color = 12345;
        Fill fill = Fill.newBuilder().setColor(color).build();
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(4321).build();
        StyleProvider styleProvider;

        styleProvider = new StyleProvider(Style.getDefaultInstance(), mockAssetProvider);
        assertThat(styleProvider.createBackground()).isNull();

        styleProvider = new StyleProvider(
                Style.newBuilder().setBackground(fill).setRoundedCorners(roundedCorners).build(),
                mockAssetProvider);
        Drawable backgroundDrawable = styleProvider.createBackground();
        assertThat(backgroundDrawable).isInstanceOf(ColorDrawable.class);

        ColorDrawable background = (ColorDrawable) backgroundDrawable;
        assertThat(background.getColor()).isEqualTo(color);
    }

    @Test
    public void testCreatePreLoadFill() {
        int color = 12345;
        Fill fill = Fill.newBuilder().setColor(color).build();
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(4321).build();
        StyleProvider styleProvider;

        styleProvider = new StyleProvider(Style.getDefaultInstance(), mockAssetProvider);
        assertThat(styleProvider.createPreLoadFill()).isNull();

        styleProvider = new StyleProvider(
                Style.newBuilder()
                        .setImageLoadingSettings(
                                ImageLoadingSettings.newBuilder().setPreLoadFill(fill).build())
                        .setRoundedCorners(roundedCorners)
                        .build(),
                mockAssetProvider);
        Drawable preLoadFillDrawable = styleProvider.createPreLoadFill();
        assertThat(preLoadFillDrawable).isInstanceOf(ColorDrawable.class);

        ColorDrawable background = (ColorDrawable) preLoadFillDrawable;
        assertThat(background.getColor()).isEqualTo(color);
    }

    @Test
    public void testEqualsAndHashCode() {
        Style style = Style.newBuilder().setColor(123).build();
        Style sameStyle = Style.newBuilder().setColor(123).build();
        Style differentStyle = Style.newBuilder().setHeight(456).build();

        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        StyleProvider sameStyleProvider = new StyleProvider(sameStyle, mockAssetProvider);
        StyleProvider differentStyleProvider = new StyleProvider(differentStyle, mockAssetProvider);

        assertThat(styleProvider).isEqualTo(sameStyleProvider);
        assertThat(styleProvider.hashCode()).isEqualTo(sameStyleProvider.hashCode());
        assertThat(styleProvider).isNotEqualTo(differentStyleProvider);
    }

    private void verifyPadding(View view, EdgeWidths padding) {
        assertThat(view.getPaddingTop())
                .isEqualTo((int) LayoutUtils.dpToPx(padding.getTop(), context));
        assertThat(view.getPaddingBottom())
                .isEqualTo((int) LayoutUtils.dpToPx(padding.getBottom(), context));
        assertThat(view.getPaddingStart())
                .isEqualTo((int) LayoutUtils.dpToPx(padding.getStart(), context));
        assertThat(view.getPaddingEnd())
                .isEqualTo((int) LayoutUtils.dpToPx(padding.getEnd(), context));
    }

    private void verifyMargins(MarginLayoutParams params, EdgeWidths margins) {
        assertThat(params.getMarginStart())
                .isEqualTo((int) LayoutUtils.dpToPx(margins.getStart(), context));
        assertThat(params.getMarginEnd())
                .isEqualTo((int) LayoutUtils.dpToPx(margins.getEnd(), context));
        assertThat(params.topMargin).isEqualTo((int) LayoutUtils.dpToPx(margins.getTop(), context));
        assertThat(params.bottomMargin)
                .isEqualTo((int) LayoutUtils.dpToPx(margins.getBottom(), context));
        assertThat(params.leftMargin)
                .isEqualTo((int) LayoutUtils.dpToPx(margins.getStart(), context));
        assertThat(params.rightMargin)
                .isEqualTo((int) LayoutUtils.dpToPx(margins.getEnd(), context));
    }
}

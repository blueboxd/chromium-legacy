// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.GridRowAdapter.KeySupplier;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.ui.GridRowView;
import com.google.search.now.ui.piet.BindingRefsProto.ElementBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.GridCellWidthBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.StyleBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.GridCell;
import com.google.search.now.ui.piet.ElementsProto.GridCellWidth;
import com.google.search.now.ui.piet.ElementsProto.GridCellWidth.ContentWidth;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;

/** Tests of the {@link GridRowAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class GridRowAdapterTest {
    private static final String GRID_STYLE_ID = "cybercat";
    private static final StyleIdsStack GRID_STYLES =
            StyleIdsStack.newBuilder().addStyleIds(GRID_STYLE_ID).build();
    private static final Element DEFAULT_ELEMENT =
            Element.newBuilder().setElementStack(ElementStack.getDefaultInstance()).build();
    private static final Content DEFAULT_CONTENT =
            Content.newBuilder().setElement(DEFAULT_ELEMENT).build();
    private static final Content TEXT_CONTENT =
            Content.newBuilder()
                    .setElement(Element.newBuilder().setTextElement(
                            TextElement.newBuilder().setParameterizedText(
                                    ParameterizedText.newBuilder().setText("TheGrid"))))
                    .build();
    private static final String BINDING_ID = "stripes";
    private static final ElementBindingRef ELEMENT_BINDING =
            ElementBindingRef.newBuilder().setBindingId(BINDING_ID).build();
    private static final Content BOUND_CONTENTS =
            Content.newBuilder().setBoundElement(ELEMENT_BINDING).build();
    private static final Element GRID_ROW_WITH_BOUND_CELL =
            Element.newBuilder()
                    .setGridRow(GridRow.newBuilder().addCells(
                            GridCell.newBuilder().setContent(BOUND_CONTENTS)))
                    .build();

    private Context context;
    private AdapterParameters adapterParameters;

    @Mock
    private ActionHandler actionHandler;
    @Mock
    private FrameContext frameContext;
    @Mock
    private StyleProvider styleProvider;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;

    private GridRowAdapter adapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(assetProvider.isRtL()).thenReturn(false);
        when(assetProvider.isRtLSupplier()).thenReturn(Suppliers.of(false));
        when(frameContext.makeStyleFor(GRID_STYLES)).thenReturn(styleProvider);
        when(frameContext.getActionHandler()).thenReturn(actionHandler);
        when(frameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(frameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        when(styleProvider.getPadding()).thenReturn(EdgeWidths.getDefaultInstance());
        when(styleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());

        adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), hostProviders, new FakeClock(), false, false);

        when(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(adapterParameters.defaultStyleProvider);

        adapter = new KeySupplier().getAdapter(context, adapterParameters);
    }

    @Test
    public void testViewDoesNotClip() {
        assertThat(adapter.getBaseView().getClipToPadding()).isFalse();
    }

    @Test
    public void testOnCreateAdapter_makesRow() {
        // We create an adapter for the inline content, but not for the bound content.
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(BOUND_CONTENTS))
                                .build();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getBaseline()).isEqualTo(-1);
        assertThat(adapter.childAdapters).hasSize(1);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
    }

    @Test
    public void testOnCreateAdapter_missingContentIsException() {
        GridRow model = GridRow.newBuilder().addCells(GridCell.getDefaultInstance()).build();

        assertThatRunnable(() -> adapter.createAdapter(asElement(model), frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled Content type: CONTENTTYPE_NOT_SET");
    }

    @Test
    public void testOnCreateAdapter_setsGridRowStyles() {
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .build();

        adapter.createAdapter(asElement(model), frameContext);

        verify(frameContext).makeStyleFor(GRID_STYLES);
        verify(styleProvider).applyElementStyles(adapter);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthDefaultsToWeight() {
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT).clearWidth())
                        .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo(0);
        assertThat(params.weight).isEqualTo(1.0f);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthDp() {
        int widthDp = 123;
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setContent(DEFAULT_CONTENT)
                                          .setWidth(GridCellWidth.newBuilder().setDp(widthDp)))
                        .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(widthDp, context));
        assertThat(params.weight).isEqualTo(0.0f);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthWeight() {
        int widthWeight = 321;
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder()
                                                  .setContent(DEFAULT_CONTENT)
                                                  .setWidth(GridCellWidth.newBuilder().setWeight(
                                                          widthWeight)))
                                .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.weight).isEqualTo((float) widthWeight);
        assertThat(params.width).isEqualTo(0);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthBinding() {
        GridCellWidthBindingRef widthBindingRef =
                GridCellWidthBindingRef.newBuilder().setBindingId("fatcat").build();
        int widthDp = 222;
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder()
                                                  .setContent(DEFAULT_CONTENT)
                                                  .setWidthBinding(widthBindingRef))
                                .build();
        when(frameContext.getGridCellWidthFromBinding(widthBindingRef))
                .thenReturn(GridCellWidth.newBuilder().setDp(widthDp).build());

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(widthDp, context));
        assertThat(params.weight).isEqualTo(0.0f);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_invalidContentWidth() {
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setContent(DEFAULT_CONTENT)
                                          .setWidth(GridCellWidth.newBuilder().setContentWidth(
                                                  ContentWidth.INVALID_CONTENT_WIDTH)))
                        .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(params.weight).isEqualTo(0.0f);
        verify(frameContext)
                .reportMessage(MessageType.WARNING, ErrorCode.ERR_GRID_CELL_WIDTH_WITHOUT_CONTENTS,
                        "Invalid content width: INVALID_CONTENT_WIDTH");
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthOfChildAdapter() {
        StyleIdsStack childStyles = StyleIdsStack.newBuilder().addStyleIds("child").build();
        GridRow model =
                GridRow.newBuilder()
                        .addCells(
                                GridCell.newBuilder()
                                        .setWidth(GridCellWidth.newBuilder().setContentWidth(
                                                ContentWidth.CONTENT_WIDTH))
                                        .setContent(Content.newBuilder().setElement(
                                                // clang-format off
                                    Element.newBuilder()
                                            .setStyleReferences(childStyles)
                                            .setImageElement(
                                                    ImageElement.newBuilder().setImage(
                                                            Image.getDefaultInstance()))
                                                // clang-format on
                                                )))
                        .build();

        StyleProvider childStyleProvider = mock(StyleProvider.class);
        when(childStyleProvider.hasWidth()).thenReturn(true);
        when(childStyleProvider.getWidthSpecPx(context)).thenReturn(456);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(childStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        when(frameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(456, context));
        assertThat(params.weight).isEqualTo(0.0f);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_heightOfChildAdapter() {
        StyleIdsStack childStyles = StyleIdsStack.newBuilder().addStyleIds("child").build();
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(Content.newBuilder().setElement(
                                Element.newBuilder()
                                        .setStyleReferences(childStyles)
                                        .setImageElement(ImageElement.newBuilder().setImage(
                                                Image.getDefaultInstance())))))
                        .build();

        StyleProvider childStyleProvider = mock(StyleProvider.class);
        when(childStyleProvider.hasHeight()).thenReturn(true);
        when(childStyleProvider.getHeightSpecPx(context)).thenReturn(123);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(childStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        when(frameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.height).isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_heightOfChildAdapterNotDefined() {
        StyleIdsStack childStyles = StyleIdsStack.newBuilder().addStyleIds("child").build();
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(Content.newBuilder().setElement(
                                Element.newBuilder().setImageElement(
                                        ImageElement.newBuilder().setImage(
                                                Image.getDefaultInstance())))))
                        .build();

        StyleProvider childStyleProvider = mock(StyleProvider.class);
        when(childStyleProvider.hasHeight()).thenReturn(false);
        when(childStyleProvider.getHeightSpecPx(context)).thenReturn(DIMENSION_NOT_SET);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());

        when(frameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.height).isEqualTo(LayoutParams.MATCH_PARENT);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_margins() {
        StyleIdsStack childStyles = StyleIdsStack.newBuilder().addStyleIds("child").build();
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(Content.newBuilder().setElement(
                                Element.newBuilder()
                                        .setStyleReferences(childStyles)
                                        .setElementStack(ElementStack.getDefaultInstance()))))
                        .build();

        StyleProvider childStyleProvider = mock(StyleProvider.class);
        when(childStyleProvider.getPadding()).thenReturn(EdgeWidths.getDefaultInstance());
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());

        when(frameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        verify(childStyleProvider)
                .applyMargins(context,
                        (MarginLayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams());
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_verticalGravityCenter() {
        StyleIdsStack centerVertical =
                StyleIdsStack.newBuilder().addStyleIds("center_vertical").build();
        StyleProvider centerVerticalProvider = mock(StyleProvider.class);
        when(frameContext.makeStyleFor(centerVertical)).thenReturn(centerVerticalProvider);
        when(centerVerticalProvider.getGravityVertical(anyInt()))
                .thenReturn(Gravity.CENTER_VERTICAL);
        GridRow gridRowTop =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(centerVertical))))
                        .build();

        adapter.createAdapter(asElement(gridRowTop), frameContext);
        adapter.bindModel(asElement(gridRowTop), frameContext);

        ElementStackAdapter cellAdapter = (ElementStackAdapter) adapter.childAdapters.get(0);
        LayoutParams params = (LinearLayout.LayoutParams) cellAdapter.getView().getLayoutParams();
        assertThat(params.gravity).isEqualTo(Gravity.CENTER_VERTICAL);
    }

    @Test
    public void testOnBindModel_collapsibleCells_valid() {
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setContentWidth(
                                                  ContentWidth.CONTENT_WIDTH))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(gridRow), frameContext);
        adapter.bindModel(asElement(gridRow), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(adapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testOnBindModel_collapsibleCells_multipleCollapsible() {
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setDp(456)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(gridRow), frameContext);
        adapter.bindModel(asElement(gridRow), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(1).getLayoutParams().width).isEqualTo(456);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testOnBindModel_collapsibleCells_mixingCollapsibleAndWeight() {
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setWeight(4))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(gridRow), frameContext);
        adapter.bindModel(asElement(gridRow), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(1).getLayoutParams().width).isEqualTo(0);
        assertThat(
                ((LinearLayout.LayoutParams) adapter.getBaseView().getChildAt(1).getLayoutParams())
                        .weight)
                .isEqualTo(4.0f);
        assertThat(adapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testOnBindModel_recreatesBindingCells() {
        Element cellWithOneElement =
                Element.newBuilder()
                        .setElementList(ElementList.newBuilder().addContents(
                                Content.newBuilder().setElement(
                                        Element.newBuilder().setElementStack(
                                                ElementStack.getDefaultInstance()))))
                        .build();
        Element cellWithTwoElements =
                Element.newBuilder()
                        .setElementList(
                                ElementList.newBuilder()
                                        .addContents(Content.newBuilder().setElement(
                                                Element.newBuilder().setElementStack(
                                                        ElementStack.getDefaultInstance())))
                                        .addContents(Content.newBuilder().setElement(
                                                Element.newBuilder().setElementStack(
                                                        ElementStack.getDefaultInstance()))))
                        .build();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithOneElement).build());
        adapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, frameContext);
        // The cell adapter has not been created yet
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithTwoElements).build());
        adapter.bindModel(GRID_ROW_WITH_BOUND_CELL, frameContext);
        // The cell adapter creates its one view on bind.
        assertThat(((LinearLayout) adapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(2);

        adapter.unbindModel();
        // The cell adapter has been released.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithOneElement).build());
        adapter.bindModel(GRID_ROW_WITH_BOUND_CELL, frameContext);
        // The cell adapter can bind to a different model.
        assertThat(((LinearLayout) adapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(1);
    }

    @Test
    public void testOnBindModel_visibilityGone() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        adapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, frameContext);
        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setVisibility(Visibility.GONE)
                                    .build());

        adapter.bindModel(GRID_ROW_WITH_BOUND_CELL, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_noContent() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        adapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, frameContext);
        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING_ID).build());

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_optionalAbsent() {
        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        adapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, frameContext);

        ElementBindingRef optionalBinding = ELEMENT_BINDING.toBuilder().setIsOptional(true).build();
        GridRow optionalBindingRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(
                                Content.newBuilder().setBoundElement(optionalBinding)))
                        .build();
        when(frameContext.getElementBindingValue(optionalBinding))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING_ID).build());

        adapter.bindModel(asElement(optionalBindingRow), frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_throwsExceptionOnCellCountMismatch() {
        GridRow gridRowWithTwoElements =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                        .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                        .build();

        GridRow gridRowWithOneElement =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(gridRowWithTwoElements), frameContext);

        assertThatRunnable(() -> adapter.bindModel(asElement(gridRowWithOneElement), frameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Internal error in adapters per content");
    }

    @Test
    public void testOnBindModel_setsStylesOnlyIfBindingIsDefined() {
        GridRow gridRow = GridRow.newBuilder()
                                  .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                  .build();

        adapter.createAdapter(asElement(gridRow, GRID_STYLES), frameContext);
        verify(frameContext).makeStyleFor(GRID_STYLES);

        // When we bind a new model, the style does not change.
        StyleIdsStack otherStyles = StyleIdsStack.newBuilder().addStyleIds("ignored").build();

        adapter.bindModel(asElement(gridRow, otherStyles), frameContext);
        verify(frameContext, never()).makeStyleFor(otherStyles);

        // If we bind a model that has a style binding, then the style does get re-applied.
        StyleIdsStack styleWithBinding =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("homewardbound"))
                        .build();
        StyleProvider otherStyleProvider = mock(StyleProvider.class);
        when(frameContext.makeStyleFor(styleWithBinding)).thenReturn(otherStyleProvider);

        adapter.bindModel(asElement(gridRow, styleWithBinding), frameContext);
        verify(frameContext).makeStyleFor(styleWithBinding);
        verify(otherStyleProvider).applyElementStyles(adapter);
    }

    @Test
    public void testOnBindModel_collapsibleCells_valid_boundWidth() {
        GridCellWidthBindingRef widthBinding =
                GridCellWidthBindingRef.newBuilder().setBindingId("width").build();
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setContentWidth(
                                                  ContentWidth.CONTENT_WIDTH))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidthBinding(widthBinding)
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .build();

        when(frameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder().setDp(456).build());
        adapter.createAdapter(asElement(gridRow), frameContext);

        when(frameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder()
                                    .setContentWidth(ContentWidth.CONTENT_WIDTH)
                                    .setIsCollapsible(true)
                                    .build());
        adapter.bindModel(asElement(gridRow), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(adapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testOnBindModel_collapsibleCells_multipleCollapsible_boundWidth() {
        GridCellWidthBindingRef widthBinding =
                GridCellWidthBindingRef.newBuilder().setBindingId("width").build();
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidthBinding(widthBinding)
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .build();

        when(frameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder().setDp(456).build());
        adapter.createAdapter(asElement(gridRow), frameContext);

        when(frameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder()
                                    .setContentWidth(ContentWidth.CONTENT_WIDTH)
                                    .setIsCollapsible(true)
                                    .build());
        adapter.bindModel(asElement(gridRow), frameContext);

        assertThat(adapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) adapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(adapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
    }

    @Test
    public void testUnbindModel() {
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(
                                        Content.newBuilder().setBoundElement(ELEMENT_BINDING)))
                                .build();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        adapter.unbindModel();
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.childAdapters).hasSize(1);

        // The inline adapter has been unbound.
        assertThat(adapter.childAdapters.get(0).getRawModel()).isNull();
    }

    @Test
    public void testUnbindModel_worksTwice() {
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        adapter.unbindModel();
        adapter.unbindModel();

        // assert no failure.
    }

    @Test
    public void testReleaseAdapter() {
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(
                                        Content.newBuilder().setBoundElement(ELEMENT_BINDING)))
                                .build();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        adapter.unbindModel();
        adapter.releaseAdapter();
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
    }

    @Test
    public void testReleaseAdapter_collapsibleCells() {
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .build();

        adapter.createAdapter(asElement(model), frameContext);
        adapter.bindModel(asElement(model), frameContext);

        adapter.unbindModel();
        adapter.releaseAdapter();

        assertThat(adapter.getBaseView().hasCollapsibleCells()).isFalse();
    }

    /**
     * Mini integration test to ensure that WRAP_CONTENT is set on GridRowAdapter when it is within
     * a hierarchy
     */
    @Test
    public void testCollapsibleCells_integration() {
        GridRow gridRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setDp(123))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder()
                                                            .setContentWidth(
                                                                    ContentWidth.CONTENT_WIDTH)
                                                            .setIsCollapsible(true))
                                          .setContent(TEXT_CONTENT))
                        .addCells(GridCell.newBuilder()
                                          .setWidth(GridCellWidth.newBuilder().setContentWidth(
                                                  ContentWidth.CONTENT_WIDTH))
                                          .setContent(TEXT_CONTENT))
                        .build();
        Frame frame = Frame.newBuilder()
                              .addContents(Content.newBuilder().setElement(
                                      Element.newBuilder().setElementList(
                                              ElementList.newBuilder().addContents(
                                                      Content.newBuilder().setElement(
                                                              Element.newBuilder().setGridRow(
                                                                      gridRow))))))
                              .build();
        EventLogger mockEventLogger = mock(EventLogger.class);
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(
                context, adapterParameters, actionHandler, mockEventLogger, DebugBehavior.SILENT);
        frameAdapter.bindModel(frame, 0, null, Collections.emptyList());
        LinearLayout gridRowView =
                ((LinearLayout) ((LinearLayout) frameAdapter.getView().getChildAt(0))
                                .getChildAt(0));
        assertThat(gridRowView.getChildCount()).isEqualTo(3);
        assertThat(gridRowView.getChildAt(0).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, context));
        assertThat(((GridRowView.LayoutParams) gridRowView.getChildAt(1).getLayoutParams())
                           .getIsCollapsible())
                .isTrue();
        assertThat(gridRowView.getChildAt(2).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testGetStyleIdsStack() {
        adapter.createAdapter(asElement(GridRow.getDefaultInstance(), GRID_STYLES), frameContext);
        assertThat(adapter.getElementStyleIdsStack()).isEqualTo(GRID_STYLES);
    }

    @Test
    public void testCreateViewGroup() {
        LinearLayout gridView = GridRowAdapter.createView(context, Suppliers.of(false));
        assertThat(gridView.getOrientation()).isEqualTo(LinearLayout.HORIZONTAL);
        assertThat(gridView.getLayoutParams().width).isEqualTo(LayoutParams.MATCH_PARENT);
        assertThat(gridView.getLayoutParams().height).isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testGetModelFromElement() {
        GridRow model = GridRow.newBuilder().build();

        Element elementWithModel =
                Element.newBuilder()
                        .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("spacer"))
                        .setGridRow(model)
                        .build();
        assertThat(adapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> adapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing GridRow");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> adapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing GridRow");
    }

    private static Element asElement(GridRow gridRow, StyleIdsStack styles) {
        return Element.newBuilder().setStyleReferences(styles).setGridRow(gridRow).build();
    }

    private static Element asElement(GridRow gridRow) {
        return Element.newBuilder().setStyleReferences(GRID_STYLES).setGridRow(gridRow).build();
    }
}

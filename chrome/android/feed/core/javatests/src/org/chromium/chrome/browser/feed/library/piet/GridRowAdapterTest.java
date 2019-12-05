// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.GridRowAdapter.KeySupplier;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.ui.GridRowView;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.GridCellWidthBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCell;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth.ContentWidth;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.EdgeWidths;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;

/** Tests of the {@link GridRowAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private Context mContext;
    private AdapterParameters mAdapterParameters;

    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private StyleProvider mStyleProvider;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;

    private GridRowAdapter mAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mAssetProvider.isRtL()).thenReturn(false);
        when(mAssetProvider.isRtLSupplier()).thenReturn(Suppliers.of(false));
        when(mFrameContext.makeStyleFor(GRID_STYLES)).thenReturn(mStyleProvider);
        when(mFrameContext.getActionHandler()).thenReturn(mActionHandler);
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(mFrameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        when(mStyleProvider.getPadding()).thenReturn(EdgeWidths.getDefaultInstance());
        when(mStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());

        mAdapterParameters = new AdapterParameters(
                mContext, Suppliers.of(null), mHostProviders, new FakeClock(), false, false);

        when(mFrameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(mAdapterParameters.mDefaultStyleProvider);

        mAdapter = new KeySupplier().getAdapter(mContext, mAdapterParameters);
    }

    @Test
    public void testViewDoesNotClip() {
        assertThat(mAdapter.getBaseView().getClipToPadding()).isFalse();
    }

    @Test
    public void testOnCreateAdapter_makesRow() {
        // We create an adapter for the inline content, but not for the bound content.
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(BOUND_CONTENTS))
                                .build();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getBaseline()).isEqualTo(-1);
        assertThat(mAdapter.mChildAdapters).hasSize(1);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
    }

    @Test
    public void testOnCreateAdapter_missingContentIsException() {
        GridRow model = GridRow.newBuilder().addCells(GridCell.getDefaultInstance()).build();

        assertThatRunnable(() -> mAdapter.createAdapter(asElement(model), mFrameContext))
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

        mAdapter.createAdapter(asElement(model), mFrameContext);

        verify(mFrameContext).makeStyleFor(GRID_STYLES);
        verify(mStyleProvider).applyElementStyles(mAdapter);
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_widthDefaultsToWeight() {
        GridRow model =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT).clearWidth())
                        .build();

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
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

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(widthDp, mContext));
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

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
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
        when(mFrameContext.getGridCellWidthFromBinding(widthBindingRef))
                .thenReturn(GridCellWidth.newBuilder().setDp(widthDp).build());

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(widthDp, mContext));
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

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(params.weight).isEqualTo(0.0f);
        verify(mFrameContext)
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
        when(childStyleProvider.getWidthSpecPx(mContext)).thenReturn(456);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(childStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        when(mFrameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.width).isEqualTo((int) LayoutUtils.dpToPx(456, mContext));
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
        when(childStyleProvider.getHeightSpecPx(mContext)).thenReturn(123);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(childStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        when(mFrameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
        assertThat(params.height).isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
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
        when(childStyleProvider.getHeightSpecPx(mContext)).thenReturn(DIMENSION_NOT_SET);
        when(childStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());

        when(mFrameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        LayoutParams params =
                (LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams();
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

        when(mFrameContext.makeStyleFor(childStyles)).thenReturn(childStyleProvider);

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        verify(childStyleProvider)
                .applyMargins(mContext,
                        (MarginLayoutParams) mAdapter.getBaseView()
                                .getChildAt(0)
                                .getLayoutParams());
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnCell_verticalGravityCenter() {
        StyleIdsStack centerVertical =
                StyleIdsStack.newBuilder().addStyleIds("center_vertical").build();
        StyleProvider centerVerticalProvider = mock(StyleProvider.class);
        when(mFrameContext.makeStyleFor(centerVertical)).thenReturn(centerVerticalProvider);
        when(centerVerticalProvider.getGravityVertical(anyInt()))
                .thenReturn(Gravity.CENTER_VERTICAL);
        GridRow gridRowTop =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(centerVertical))))
                        .build();

        mAdapter.createAdapter(asElement(gridRowTop), mFrameContext);
        mAdapter.bindModel(asElement(gridRowTop), mFrameContext);

        ElementStackAdapter cellAdapter = (ElementStackAdapter) mAdapter.mChildAdapters.get(0);
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

        mAdapter.createAdapter(asElement(gridRow), mFrameContext);
        mAdapter.bindModel(asElement(gridRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(mAdapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
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

        mAdapter.createAdapter(asElement(gridRow), mFrameContext);
        mAdapter.bindModel(asElement(gridRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(1).getLayoutParams().width).isEqualTo(456);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
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

        mAdapter.createAdapter(asElement(gridRow), mFrameContext);
        mAdapter.bindModel(asElement(gridRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(1).getLayoutParams().width).isEqualTo(0);
        assertThat(
                ((LinearLayout.LayoutParams) mAdapter.getBaseView().getChildAt(1).getLayoutParams())
                        .weight)
                .isEqualTo(4.0f);
        assertThat(mAdapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
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

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithOneElement).build());
        mAdapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, mFrameContext);
        // The cell adapter has not been created yet
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithTwoElements).build());
        mAdapter.bindModel(GRID_ROW_WITH_BOUND_CELL, mFrameContext);
        // The cell adapter creates its one view on bind.
        assertThat(((LinearLayout) mAdapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(2);

        mAdapter.unbindModel();
        // The cell adapter has been released.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(cellWithOneElement).build());
        mAdapter.bindModel(GRID_ROW_WITH_BOUND_CELL, mFrameContext);
        // The cell adapter can bind to a different model.
        assertThat(((LinearLayout) mAdapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(1);
    }

    @Test
    public void testOnBindModel_visibilityGone() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        mAdapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, mFrameContext);
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setVisibility(Visibility.GONE)
                                    .build());

        mAdapter.bindModel(GRID_ROW_WITH_BOUND_CELL, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_noContent() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        mAdapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, mFrameContext);
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING_ID).build());

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_optionalAbsent() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        mAdapter.createAdapter(GRID_ROW_WITH_BOUND_CELL, mFrameContext);

        ElementBindingRef optionalBinding = ELEMENT_BINDING.toBuilder().setIsOptional(true).build();
        GridRow optionalBindingRow =
                GridRow.newBuilder()
                        .addCells(GridCell.newBuilder().setContent(
                                Content.newBuilder().setBoundElement(optionalBinding)))
                        .build();
        when(mFrameContext.getElementBindingValue(optionalBinding))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING_ID).build());

        mAdapter.bindModel(asElement(optionalBindingRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
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

        mAdapter.createAdapter(asElement(gridRowWithTwoElements), mFrameContext);

        assertThatRunnable(
                () -> mAdapter.bindModel(asElement(gridRowWithOneElement), mFrameContext))
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

        mAdapter.createAdapter(asElement(gridRow, GRID_STYLES), mFrameContext);
        verify(mFrameContext).makeStyleFor(GRID_STYLES);

        // When we bind a new model, the style does not change.
        StyleIdsStack otherStyles = StyleIdsStack.newBuilder().addStyleIds("ignored").build();

        mAdapter.bindModel(asElement(gridRow, otherStyles), mFrameContext);
        verify(mFrameContext, never()).makeStyleFor(otherStyles);

        // If we bind a model that has a style binding, then the style does get re-applied.
        StyleIdsStack styleWithBinding =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("homewardbound"))
                        .build();
        StyleProvider otherStyleProvider = mock(StyleProvider.class);
        when(mFrameContext.makeStyleFor(styleWithBinding)).thenReturn(otherStyleProvider);

        mAdapter.bindModel(asElement(gridRow, styleWithBinding), mFrameContext);
        verify(mFrameContext).makeStyleFor(styleWithBinding);
        verify(otherStyleProvider).applyElementStyles(mAdapter);
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

        when(mFrameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder().setDp(456).build());
        mAdapter.createAdapter(asElement(gridRow), mFrameContext);

        when(mFrameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder()
                                    .setContentWidth(ContentWidth.CONTENT_WIDTH)
                                    .setIsCollapsible(true)
                                    .build());
        mAdapter.bindModel(asElement(gridRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(mAdapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
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

        when(mFrameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder().setDp(456).build());
        mAdapter.createAdapter(asElement(gridRow), mFrameContext);

        when(mFrameContext.getGridCellWidthFromBinding(widthBinding))
                .thenReturn(GridCellWidth.newBuilder()
                                    .setContentWidth(ContentWidth.CONTENT_WIDTH)
                                    .setIsCollapsible(true)
                                    .build());
        mAdapter.bindModel(asElement(gridRow), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(1).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
        assertThat(
                ((GridRowView.LayoutParams) mAdapter.getBaseView().getChildAt(1).getLayoutParams())
                        .getIsCollapsible())
                .isTrue();
        assertThat(mAdapter.getBaseView().getChildAt(2).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
    }

    @Test
    public void testUnbindModel() {
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(
                                        Content.newBuilder().setBoundElement(ELEMENT_BINDING)))
                                .build();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        mAdapter.unbindModel();
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.mChildAdapters).hasSize(1);

        // The inline adapter has been unbound.
        assertThat(mAdapter.mChildAdapters.get(0).getRawModel()).isNull();
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

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        mAdapter.unbindModel();
        mAdapter.unbindModel();

        // assert no failure.
    }

    @Test
    public void testReleaseAdapter() {
        GridRow model = GridRow.newBuilder()
                                .addCells(GridCell.newBuilder().setContent(DEFAULT_CONTENT))
                                .addCells(GridCell.newBuilder().setContent(
                                        Content.newBuilder().setBoundElement(ELEMENT_BINDING)))
                                .build();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(BindingValue.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        mAdapter.unbindModel();
        mAdapter.releaseAdapter();
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
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

        mAdapter.createAdapter(asElement(model), mFrameContext);
        mAdapter.bindModel(asElement(model), mFrameContext);

        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        assertThat(mAdapter.getBaseView().hasCollapsibleCells()).isFalse();
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
        FrameAdapterImpl frameAdapter = new FrameAdapterImpl(mContext, mAdapterParameters,
                mActionHandler, mockEventLogger, DebugBehavior.SILENT);
        frameAdapter.bindModel(frame, 0, null, Collections.emptyList());
        LinearLayout gridRowView =
                ((LinearLayout) ((LinearLayout) frameAdapter.getView().getChildAt(0))
                                .getChildAt(0));
        assertThat(gridRowView.getChildCount()).isEqualTo(3);
        assertThat(gridRowView.getChildAt(0).getLayoutParams().width)
                .isEqualTo((int) LayoutUtils.dpToPx(123, mContext));
        assertThat(((GridRowView.LayoutParams) gridRowView.getChildAt(1).getLayoutParams())
                           .getIsCollapsible())
                .isTrue();
        assertThat(gridRowView.getChildAt(2).getLayoutParams().width)
                .isEqualTo(LayoutParams.WRAP_CONTENT);
    }

    @Test
    public void testGetStyleIdsStack() {
        mAdapter.createAdapter(asElement(GridRow.getDefaultInstance(), GRID_STYLES), mFrameContext);
        assertThat(mAdapter.getElementStyleIdsStack()).isEqualTo(GRID_STYLES);
    }

    @Test
    public void testCreateViewGroup() {
        LinearLayout gridView = GridRowAdapter.createView(mContext, Suppliers.of(false));
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
        assertThat(mAdapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> mAdapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing GridRow");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> mAdapter.getModelFromElement(emptyElement))
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

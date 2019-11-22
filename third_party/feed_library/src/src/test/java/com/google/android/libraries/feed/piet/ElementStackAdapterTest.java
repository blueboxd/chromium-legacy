// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static java.util.Arrays.asList;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.BindingRefsProto.ElementBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests of the {@link ElementStackAdapter} */
@RunWith(RobolectricTestRunner.class)
public class ElementStackAdapterTest {
    private static final Content INLINE_CONTENT =
            Content.newBuilder()
                    .setElement(
                            Element.newBuilder().setElementList(ElementList.getDefaultInstance()))
                    .build();
    private static final Element BOUND_ELEMENT =
            Element.newBuilder().setGridRow(GridRow.getDefaultInstance()).build();
    private static final String ELEMENT_BINDING_ID = "RowBinding";
    private static final ElementBindingRef ELEMENT_BINDING =
            ElementBindingRef.newBuilder().setBindingId(ELEMENT_BINDING_ID).build();
    private static final BindingValue ELEMENT_BINDING_VALUE =
            BindingValue.newBuilder()
                    .setBindingId(ELEMENT_BINDING_ID)
                    .setElement(BOUND_ELEMENT)
                    .build();
    private static final Content BOUND_CONTENT =
            Content.newBuilder().setBoundElement(ELEMENT_BINDING).build();

    @Mock
    private FrameContext frameContext;
    @Mock
    private AssetProvider assetProvider;
    @Mock
    private StyleProvider mockStyleProvider;
    @Mock
    private HostProviders mockHostProviders;

    private Context context;
    private AdapterParameters adapterParameters;

    private ElementStackAdapter adapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(frameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(ELEMENT_BINDING_VALUE);
        when(frameContext.makeStyleFor(any())).thenReturn(mockStyleProvider);
        when(frameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(mockHostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(mockStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());

        adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), mockHostProviders, new FakeClock(), false, false);

        adapter = new ElementStackAdapter.KeySupplier().getAdapter(context, adapterParameters);
    }

    @Test
    public void testCreatesInlineAdaptersInOnCreate() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        adapter.createAdapter(element, frameContext);

        assertThat(adapter.childAdapters).hasSize(1);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
    }

    @Test
    public void testCreatesBoundAdaptersInOnBind() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        adapter.createAdapter(element, frameContext);
        adapter.bindModel(element, frameContext);

        assertThat(adapter.childAdapters).hasSize(2);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(adapter.childAdapters.get(1)).isInstanceOf(GridRowAdapter.class);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(2);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
        assertThat(adapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(adapter.childAdapters.get(1).getView());
    }

    @Test
    public void testSetsLayoutParamsOnChildForInlineAdapters() {
        Content imageContent =
                Content.newBuilder()
                        .setElement(Element.newBuilder().setImageElement(
                                ImageElement.newBuilder().setImage(Image.getDefaultInstance())))
                        .build();
        Element element = asElement(Collections.singletonList(imageContent));

        when(mockStyleProvider.hasWidth()).thenReturn(true);
        when(mockStyleProvider.hasHeight()).thenReturn(true);
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(123);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(456);
        when(mockStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        adapter.createAdapter(element, frameContext);
        adapter.bindModel(element, frameContext);

        LayoutParams inlineAdapterLayoutParams =
                adapter.childAdapters.get(0).getView().getLayoutParams();

        assertThat(inlineAdapterLayoutParams.width).isEqualTo(123);
        assertThat(inlineAdapterLayoutParams.height).isEqualTo(456);
    }

    @Test
    public void testSetsLayoutParamsOnChildForBoundAdapters() {
        ElementBindingRef imageBinding =
                ElementBindingRef.newBuilder().setBindingId("image").build();
        BindingValue imageBindingValue =
                BindingValue.newBuilder()
                        .setElement(Element.newBuilder().setImageElement(
                                ImageElement.newBuilder().setImage(Image.getDefaultInstance())))
                        .build();
        when(frameContext.getElementBindingValue(imageBinding)).thenReturn(imageBindingValue);
        Element element = asElement(Collections.singletonList(
                Content.newBuilder().setBoundElement(imageBinding).build()));

        when(mockStyleProvider.hasWidth()).thenReturn(true);
        when(mockStyleProvider.hasHeight()).thenReturn(true);
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(123);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(456);
        when(mockStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        adapter.createAdapter(element, frameContext);
        adapter.bindModel(element, frameContext);

        LayoutParams boundAdapterLayoutParams =
                adapter.childAdapters.get(0).getView().getLayoutParams();

        assertThat(boundAdapterLayoutParams.width).isEqualTo(123);
        assertThat(boundAdapterLayoutParams.height).isEqualTo(456);
    }

    @Test
    public void testSetsMarginsOnChild() {
        Element element = asElement(Collections.singletonList(INLINE_CONTENT));

        adapter.createAdapter(element, frameContext);
        adapter.bindModel(element, frameContext);

        MarginLayoutParams childAdapterLayoutParams =
                (MarginLayoutParams) adapter.childAdapters.get(0).getView().getLayoutParams();

        verify(mockStyleProvider).applyMargins(context, childAdapterLayoutParams);
    }

    @Test
    public void testGetContentsFromModel() {
        ElementStack model = ElementStack.newBuilder()
                                     .addAllContents(asList(INLINE_CONTENT, BOUND_CONTENT))
                                     .build();

        assertThat(adapter.getContentsFromModel(model)).isSameInstanceAs(model.getContentsList());
    }

    @Test
    public void testGetModelFromElement() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        assertThat(adapter.getModelFromElement(element))
                .isSameInstanceAs(element.getElementStack());
    }

    private Element asElement(List<Content> contents) {
        return Element.newBuilder()
                .setElementStack(ElementStack.newBuilder().addAllContents(contents))
                .build();
    }
}

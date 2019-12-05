// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests of the {@link ElementStackAdapter} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private FrameContext mFrameContext;
    @Mock
    private AssetProvider mAssetProvider;
    @Mock
    private StyleProvider mMockStyleProvider;
    @Mock
    private HostProviders mMockHostProviders;

    private Context mContext;
    private AdapterParameters mAdapterParameters;

    private ElementStackAdapter mAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING))
                .thenReturn(ELEMENT_BINDING_VALUE);
        when(mFrameContext.makeStyleFor(any())).thenReturn(mMockStyleProvider);
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(mMockHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mMockStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());

        mAdapterParameters = new AdapterParameters(
                mContext, Suppliers.of(null), mMockHostProviders, new FakeClock(), false, false);

        mAdapter = new ElementStackAdapter.KeySupplier().getAdapter(mContext, mAdapterParameters);
    }

    @Test
    public void testCreatesInlineAdaptersInOnCreate() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        mAdapter.createAdapter(element, mFrameContext);

        assertThat(mAdapter.mChildAdapters).hasSize(1);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
    }

    @Test
    public void testCreatesBoundAdaptersInOnBind() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        mAdapter.createAdapter(element, mFrameContext);
        mAdapter.bindModel(element, mFrameContext);

        assertThat(mAdapter.mChildAdapters).hasSize(2);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(1)).isInstanceOf(GridRowAdapter.class);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(2);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
        assertThat(mAdapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(1).getView());
    }

    @Test
    public void testSetsLayoutParamsOnChildForInlineAdapters() {
        Content imageContent =
                Content.newBuilder()
                        .setElement(Element.newBuilder().setImageElement(
                                ImageElement.newBuilder().setImage(Image.getDefaultInstance())))
                        .build();
        Element element = asElement(Collections.singletonList(imageContent));

        when(mMockStyleProvider.hasWidth()).thenReturn(true);
        when(mMockStyleProvider.hasHeight()).thenReturn(true);
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(123);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(456);
        when(mMockStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        mAdapter.createAdapter(element, mFrameContext);
        mAdapter.bindModel(element, mFrameContext);

        LayoutParams inlineAdapterLayoutParams =
                mAdapter.mChildAdapters.get(0).getView().getLayoutParams();

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
        when(mFrameContext.getElementBindingValue(imageBinding)).thenReturn(imageBindingValue);
        Element element = asElement(Collections.singletonList(
                Content.newBuilder().setBoundElement(imageBinding).build()));

        when(mMockStyleProvider.hasWidth()).thenReturn(true);
        when(mMockStyleProvider.hasHeight()).thenReturn(true);
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(123);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(456);
        when(mMockStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);

        mAdapter.createAdapter(element, mFrameContext);
        mAdapter.bindModel(element, mFrameContext);

        LayoutParams boundAdapterLayoutParams =
                mAdapter.mChildAdapters.get(0).getView().getLayoutParams();

        assertThat(boundAdapterLayoutParams.width).isEqualTo(123);
        assertThat(boundAdapterLayoutParams.height).isEqualTo(456);
    }

    @Test
    public void testSetsMarginsOnChild() {
        Element element = asElement(Collections.singletonList(INLINE_CONTENT));

        mAdapter.createAdapter(element, mFrameContext);
        mAdapter.bindModel(element, mFrameContext);

        MarginLayoutParams childAdapterLayoutParams =
                (MarginLayoutParams) mAdapter.mChildAdapters.get(0).getView().getLayoutParams();

        verify(mMockStyleProvider).applyMargins(mContext, childAdapterLayoutParams);
    }

    @Test
    public void testGetContentsFromModel() {
        ElementStack model = ElementStack.newBuilder()
                                     .addAllContents(asList(INLINE_CONTENT, BOUND_CONTENT))
                                     .build();

        assertThat(mAdapter.getContentsFromModel(model)).isSameInstanceAs(model.getContentsList());
    }

    @Test
    public void testGetModelFromElement() {
        Element element = asElement(asList(INLINE_CONTENT, BOUND_CONTENT));

        assertThat(mAdapter.getModelFromElement(element))
                .isSameInstanceAs(element.getElementStack());
    }

    private Element asElement(List<Content> contents) {
        return Element.newBuilder()
                .setElementStack(ElementStack.newBuilder().addAllContents(contents))
                .build();
    }
}

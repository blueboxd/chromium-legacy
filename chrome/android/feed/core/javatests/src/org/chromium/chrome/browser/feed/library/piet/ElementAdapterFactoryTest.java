// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateKey;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ChunkedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for the ElementAdapterFactory. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ElementAdapterFactoryTest {
    @Mock
    private FrameContext mFrameContext;

    @Mock
    private CustomElementAdapter mCustomElementAdapter;
    @Mock
    private ChunkedTextElementAdapter mChunkedTextElementAdapter;
    @Mock
    private ParameterizedTextElementAdapter mParameterizedTextElementAdapter;
    @Mock
    private ImageElementAdapter mImageElementAdapter;
    @Mock
    private GridRowAdapter mGridRowAdapter;
    @Mock
    private ElementListAdapter mElementListAdapter;
    @Mock
    private ElementAdapter<? extends View, ?> mTemplateAdapter;
    @Mock
    private ElementStackAdapter mElementStackAdapter;

    @Mock
    private AdapterFactory<CustomElementAdapter, CustomElement> mCustomElementFactory;

    @Mock
    private AdapterFactory<ChunkedTextElementAdapter, Element> mChunkedTextElementFactory;

    @Mock
    private AdapterFactory<ParameterizedTextElementAdapter, Element>
            mParameterizedTextElementFactory;

    @Mock
    private AdapterFactory<ImageElementAdapter, ImageElement> mImageElementFactory;

    @Mock
    private AdapterFactory<GridRowAdapter, GridRow> mGridRowFactory;

    @Mock
    private AdapterFactory<ElementListAdapter, ElementList> mElementListFactory;

    @Mock
    private AdapterFactory<ElementStackAdapter, ElementStack> mElementStackFactory;

    @Mock
    private KeyedRecyclerPool<ElementAdapter<? extends View, ?>> mTemplateRecyclerPool;

    private final List<AdapterFactory<?, ?>> mAdapterFactories = new ArrayList<>();

    private ElementAdapterFactory mAdapterFactory;

    @Before
    public void setUp() {
        initMocks(this);
        when(mFrameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        mAdapterFactory = new ElementAdapterFactory(mCustomElementFactory,
                mChunkedTextElementFactory, mParameterizedTextElementFactory, mImageElementFactory,
                mGridRowFactory, mElementListFactory, mElementStackFactory, mTemplateRecyclerPool);
        when(mCustomElementFactory.get(any(CustomElement.class), eq(mFrameContext)))
                .thenReturn(mCustomElementAdapter);
        when(mChunkedTextElementFactory.get(any(Element.class), eq(mFrameContext)))
                .thenReturn(mChunkedTextElementAdapter);
        when(mParameterizedTextElementFactory.get(any(Element.class), eq(mFrameContext)))
                .thenReturn(mParameterizedTextElementAdapter);
        when(mImageElementFactory.get(any(ImageElement.class), eq(mFrameContext)))
                .thenReturn(mImageElementAdapter);
        when(mGridRowFactory.get(any(GridRow.class), eq(mFrameContext)))
                .thenReturn(mGridRowAdapter);
        when(mElementListFactory.get(any(ElementList.class), eq(mFrameContext)))
                .thenReturn(mElementListAdapter);
        when(mElementStackFactory.get(any(ElementStack.class), eq(mFrameContext)))
                .thenReturn(mElementStackAdapter);
        mAdapterFactories.add(mCustomElementFactory);
        mAdapterFactories.add(mChunkedTextElementFactory);
        mAdapterFactories.add(mParameterizedTextElementFactory);
        mAdapterFactories.add(mImageElementFactory);
        mAdapterFactories.add(mGridRowFactory);
        mAdapterFactories.add(mElementListFactory);
        mAdapterFactories.add(mElementStackFactory);
    }

    @Test
    public void testCreateAdapterForElement_customElement() {
        CustomElement model = CustomElement.getDefaultInstance();
        Element element = Element.newBuilder().setCustomElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mCustomElementAdapter);
        verify(mCustomElementFactory).get(model, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_chunkedTextElement() {
        TextElement model =
                TextElement.newBuilder().setChunkedText(ChunkedText.getDefaultInstance()).build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mChunkedTextElementAdapter);
        verify(mChunkedTextElementFactory).get(element, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_chunkedTextBindingElement() {
        TextElement model =
                TextElement.newBuilder()
                        .setChunkedTextBinding(ChunkedTextBindingRef.getDefaultInstance())
                        .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mChunkedTextElementAdapter);
        verify(mChunkedTextElementFactory).get(element, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_parameterizedTextElement() {
        TextElement model = TextElement.newBuilder()
                                    .setParameterizedText(ParameterizedText.getDefaultInstance())
                                    .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mParameterizedTextElementAdapter);
        verify(mParameterizedTextElementFactory).get(element, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_parameterizedTextBindingElement() {
        TextElement model = TextElement.newBuilder()
                                    .setParameterizedTextBinding(
                                            ParameterizedTextBindingRef.getDefaultInstance())
                                    .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mParameterizedTextElementAdapter);
        verify(mParameterizedTextElementFactory).get(element, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_invalidTextElement() {
        TextElement model = TextElement.getDefaultInstance();
        Element element = Element.newBuilder().setTextElement(model).build();

        assertThatRunnable(() -> mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unsupported TextElement type");
    }

    @Test
    public void testCreateAdapterForElement_imageElement() {
        ImageElement model = ImageElement.getDefaultInstance();
        Element element = Element.newBuilder().setImageElement(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mImageElementAdapter);
        verify(mImageElementFactory).get(model, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_gridRow() {
        GridRow model = GridRow.getDefaultInstance();
        Element element = Element.newBuilder().setGridRow(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mGridRowAdapter);
        verify(mGridRowFactory).get(model, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_elementList() {
        ElementList model = ElementList.getDefaultInstance();
        Element element = Element.newBuilder().setElementList(model).build();
        assertThat(mAdapterFactory.createAdapterForElement(element, mFrameContext))
                .isSameInstanceAs(mElementListAdapter);
        verify(mElementListFactory).get(model, mFrameContext);
    }

    @Test
    public void testCreateAdapterForElement_unsupported() {
        assertThatRunnable(()
                                   -> mAdapterFactory.createAdapterForElement(
                                           Element.getDefaultInstance(), mFrameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unsupported Element type");
    }

    @Test
    public void testReleaseAdapter_template() {
        TemplateKey templateKey = new TemplateKey(Template.getDefaultInstance(), null, null);
        when(mTemplateAdapter.getKey()).thenReturn(templateKey);

        mAdapterFactory.releaseAdapter(mTemplateAdapter);

        // We unbind the adapter but do not release it so it can be created quickly again.
        verify(mTemplateAdapter).unbindModel();
        verify(mTemplateAdapter, never()).releaseAdapter();
        verify(mTemplateRecyclerPool).put(templateKey, mTemplateAdapter);
    }

    @Test
    public void testReleaseAdapter_custom() {
        mAdapterFactory.releaseAdapter(mCustomElementAdapter);
        verify(mCustomElementFactory).release(mCustomElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_chunkedText() {
        mAdapterFactory.releaseAdapter(mChunkedTextElementAdapter);
        verify(mChunkedTextElementFactory).release(mChunkedTextElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_parameterizedText() {
        mAdapterFactory.releaseAdapter(mParameterizedTextElementAdapter);
        verify(mParameterizedTextElementFactory).release(mParameterizedTextElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_imageElement() {
        mAdapterFactory.releaseAdapter(mImageElementAdapter);
        verify(mImageElementFactory).release(mImageElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_gridRow() {
        mAdapterFactory.releaseAdapter(mGridRowAdapter);
        verify(mGridRowFactory).release(mGridRowAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_list() {
        mAdapterFactory.releaseAdapter(mElementListAdapter);
        verify(mElementListFactory).release(mElementListAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_stacked() {
        mAdapterFactory.releaseAdapter(mElementStackAdapter);
        verify(mElementStackFactory).release(mElementStackAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testPurgeRecyclerPools() {
        mAdapterFactory.purgeRecyclerPools();
        for (AdapterFactory<?, ?> adapterFactory : mAdapterFactories) {
            verify(adapterFactory).purgeRecyclerPool();
        }
    }

    @Test
    public void testPurgeTemplateRecyclerPool() {
        Template template = Template.newBuilder()
                                    .setTemplateId("template")
                                    .setElement(Element.newBuilder().setElementList(
                                            ElementList.getDefaultInstance()))
                                    .build();
        List<PietSharedState> sharedStates = Collections.emptyList();
        when(mTemplateAdapter.getKey()).thenReturn(new TemplateKey(template, sharedStates, null));
        when(mFrameContext.getPietSharedStates()).thenReturn(sharedStates);

        // Release adapter to populate the recycler pool
        mAdapterFactory.releaseAdapter(mTemplateAdapter);

        verify(mTemplateRecyclerPool).put(mTemplateAdapter.getKey(), mTemplateAdapter);

        // Purge the recycler pools.
        mAdapterFactory.purgeRecyclerPools();

        verify(mTemplateRecyclerPool).clear();
    }

    private void testNoOtherFactoryInteractions() {
        for (AdapterFactory<?, ?> adapterFactory : mAdapterFactories) {
            verifyNoMoreInteractions(adapterFactory);
        }
    }
}

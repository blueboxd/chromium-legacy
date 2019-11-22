// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
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

import android.view.View;

import com.google.android.libraries.feed.piet.TemplateBinder.TemplateKey;
import com.google.search.now.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.PietProto.PietSharedState;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.ui.piet.TextProto.ChunkedText;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for the ElementAdapterFactory. */
@RunWith(RobolectricTestRunner.class)
public class ElementAdapterFactoryTest {
    @Mock
    private FrameContext frameContext;

    @Mock
    private CustomElementAdapter customElementAdapter;
    @Mock
    private ChunkedTextElementAdapter chunkedTextElementAdapter;
    @Mock
    private ParameterizedTextElementAdapter parameterizedTextElementAdapter;
    @Mock
    private ImageElementAdapter imageElementAdapter;
    @Mock
    private GridRowAdapter gridRowAdapter;
    @Mock
    private ElementListAdapter elementListAdapter;
    @Mock
    private ElementAdapter<? extends View, ?> templateAdapter;
    @Mock
    private ElementStackAdapter elementStackAdapter;

    @Mock
    private AdapterFactory<CustomElementAdapter, CustomElement> customElementFactory;

    @Mock
    private AdapterFactory<ChunkedTextElementAdapter, Element> chunkedTextElementFactory;

    @Mock
    private AdapterFactory<ParameterizedTextElementAdapter, Element>
            parameterizedTextElementFactory;

    @Mock
    private AdapterFactory<ImageElementAdapter, ImageElement> imageElementFactory;

    @Mock
    private AdapterFactory<GridRowAdapter, GridRow> gridRowFactory;

    @Mock
    private AdapterFactory<ElementListAdapter, ElementList> elementListFactory;

    @Mock
    private AdapterFactory<ElementStackAdapter, ElementStack> elementStackFactory;

    @Mock
    private KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool;

    private final List<AdapterFactory<?, ?>> adapterFactories = new ArrayList<>();

    private ElementAdapterFactory adapterFactory;

    @Before
    public void setUp() {
        initMocks(this);
        when(frameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        adapterFactory = new ElementAdapterFactory(customElementFactory, chunkedTextElementFactory,
                parameterizedTextElementFactory, imageElementFactory, gridRowFactory,
                elementListFactory, elementStackFactory, templateRecyclerPool);
        when(customElementFactory.get(any(CustomElement.class), eq(frameContext)))
                .thenReturn(customElementAdapter);
        when(chunkedTextElementFactory.get(any(Element.class), eq(frameContext)))
                .thenReturn(chunkedTextElementAdapter);
        when(parameterizedTextElementFactory.get(any(Element.class), eq(frameContext)))
                .thenReturn(parameterizedTextElementAdapter);
        when(imageElementFactory.get(any(ImageElement.class), eq(frameContext)))
                .thenReturn(imageElementAdapter);
        when(gridRowFactory.get(any(GridRow.class), eq(frameContext))).thenReturn(gridRowAdapter);
        when(elementListFactory.get(any(ElementList.class), eq(frameContext)))
                .thenReturn(elementListAdapter);
        when(elementStackFactory.get(any(ElementStack.class), eq(frameContext)))
                .thenReturn(elementStackAdapter);
        adapterFactories.add(customElementFactory);
        adapterFactories.add(chunkedTextElementFactory);
        adapterFactories.add(parameterizedTextElementFactory);
        adapterFactories.add(imageElementFactory);
        adapterFactories.add(gridRowFactory);
        adapterFactories.add(elementListFactory);
        adapterFactories.add(elementStackFactory);
    }

    @Test
    public void testCreateAdapterForElement_customElement() {
        CustomElement model = CustomElement.getDefaultInstance();
        Element element = Element.newBuilder().setCustomElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(customElementAdapter);
        verify(customElementFactory).get(model, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_chunkedTextElement() {
        TextElement model =
                TextElement.newBuilder().setChunkedText(ChunkedText.getDefaultInstance()).build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(chunkedTextElementAdapter);
        verify(chunkedTextElementFactory).get(element, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_chunkedTextBindingElement() {
        TextElement model =
                TextElement.newBuilder()
                        .setChunkedTextBinding(ChunkedTextBindingRef.getDefaultInstance())
                        .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(chunkedTextElementAdapter);
        verify(chunkedTextElementFactory).get(element, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_parameterizedTextElement() {
        TextElement model = TextElement.newBuilder()
                                    .setParameterizedText(ParameterizedText.getDefaultInstance())
                                    .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(parameterizedTextElementAdapter);
        verify(parameterizedTextElementFactory).get(element, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_parameterizedTextBindingElement() {
        TextElement model = TextElement.newBuilder()
                                    .setParameterizedTextBinding(
                                            ParameterizedTextBindingRef.getDefaultInstance())
                                    .build();
        Element element = Element.newBuilder().setTextElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(parameterizedTextElementAdapter);
        verify(parameterizedTextElementFactory).get(element, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_invalidTextElement() {
        TextElement model = TextElement.getDefaultInstance();
        Element element = Element.newBuilder().setTextElement(model).build();

        assertThatRunnable(() -> adapterFactory.createAdapterForElement(element, frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unsupported TextElement type");
    }

    @Test
    public void testCreateAdapterForElement_imageElement() {
        ImageElement model = ImageElement.getDefaultInstance();
        Element element = Element.newBuilder().setImageElement(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(imageElementAdapter);
        verify(imageElementFactory).get(model, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_gridRow() {
        GridRow model = GridRow.getDefaultInstance();
        Element element = Element.newBuilder().setGridRow(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(gridRowAdapter);
        verify(gridRowFactory).get(model, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_elementList() {
        ElementList model = ElementList.getDefaultInstance();
        Element element = Element.newBuilder().setElementList(model).build();
        assertThat(adapterFactory.createAdapterForElement(element, frameContext))
                .isSameInstanceAs(elementListAdapter);
        verify(elementListFactory).get(model, frameContext);
    }

    @Test
    public void testCreateAdapterForElement_unsupported() {
        assertThatRunnable(()
                                   -> adapterFactory.createAdapterForElement(
                                           Element.getDefaultInstance(), frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unsupported Element type");
    }

    @Test
    public void testReleaseAdapter_template() {
        TemplateKey templateKey = new TemplateKey(Template.getDefaultInstance(), null, null);
        when(templateAdapter.getKey()).thenReturn(templateKey);

        adapterFactory.releaseAdapter(templateAdapter);

        // We unbind the adapter but do not release it so it can be created quickly again.
        verify(templateAdapter).unbindModel();
        verify(templateAdapter, never()).releaseAdapter();
        verify(templateRecyclerPool).put(templateKey, templateAdapter);
    }

    @Test
    public void testReleaseAdapter_custom() {
        adapterFactory.releaseAdapter(customElementAdapter);
        verify(customElementFactory).release(customElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_chunkedText() {
        adapterFactory.releaseAdapter(chunkedTextElementAdapter);
        verify(chunkedTextElementFactory).release(chunkedTextElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_parameterizedText() {
        adapterFactory.releaseAdapter(parameterizedTextElementAdapter);
        verify(parameterizedTextElementFactory).release(parameterizedTextElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_imageElement() {
        adapterFactory.releaseAdapter(imageElementAdapter);
        verify(imageElementFactory).release(imageElementAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_gridRow() {
        adapterFactory.releaseAdapter(gridRowAdapter);
        verify(gridRowFactory).release(gridRowAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_list() {
        adapterFactory.releaseAdapter(elementListAdapter);
        verify(elementListFactory).release(elementListAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testReleaseAdapter_stacked() {
        adapterFactory.releaseAdapter(elementStackAdapter);
        verify(elementStackFactory).release(elementStackAdapter);
        testNoOtherFactoryInteractions();
    }

    @Test
    public void testPurgeRecyclerPools() {
        adapterFactory.purgeRecyclerPools();
        for (AdapterFactory<?, ?> adapterFactory : adapterFactories) {
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
        when(templateAdapter.getKey()).thenReturn(new TemplateKey(template, sharedStates, null));
        when(frameContext.getPietSharedStates()).thenReturn(sharedStates);

        // Release adapter to populate the recycler pool
        adapterFactory.releaseAdapter(templateAdapter);

        verify(templateRecyclerPool).put(templateAdapter.getKey(), templateAdapter);

        // Purge the recycler pools.
        adapterFactory.purgeRecyclerPools();

        verify(templateRecyclerPool).clear();
    }

    private void testNoOtherFactoryInteractions() {
        for (AdapterFactory<?, ?> adapterFactory : adapterFactories) {
            verifyNoMoreInteractions(adapterFactory);
        }
    }
}

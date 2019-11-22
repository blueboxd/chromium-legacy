// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.TextElementAdapter.TextElementKey;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link TextElementAdapter} instance of the {@link AdapterFactory}. */
@RunWith(RobolectricTestRunner.class)
public class ParameterizedTextElementAdapterFactoryTest {
    private static final String TEXT_LINE_CONTENT = "Content";

    @Mock
    private FrameContext frameContext;
    @Mock
    ParameterizedTextElementAdapter.KeySupplier keySupplier;
    @Mock
    ParameterizedTextElementAdapter adapter;
    @Mock
    ParameterizedTextElementAdapter adapter2;

    private AdapterParameters adapterParameters;
    private Context context;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        adapterParameters = new AdapterParameters(null, null,
                new HostProviders(mock(AssetProvider.class), null, null, null), new FakeClock(),
                false, false);
        when(keySupplier.getAdapter(context, adapterParameters))
                .thenReturn(adapter)
                .thenReturn(adapter2);
    }

    @Test
    public void testKeySupplier() {
        String styleId = "text";
        StyleIdsStack style = StyleIdsStack.newBuilder().addStyleIds(styleId).build();
        Font font = Font.newBuilder().setSize(123).setItalic(true).build();
        Element model = Element.newBuilder()
                                .setStyleReferences(style)
                                .setTextElement(TextElement.getDefaultInstance())
                                .build();
        StyleProvider styleProvider = mock(StyleProvider.class);
        when(frameContext.makeStyleFor(style)).thenReturn(styleProvider);
        when(styleProvider.getFont()).thenReturn(font);
        ParameterizedTextElementAdapter.KeySupplier keySupplier =
                new ParameterizedTextElementAdapter.KeySupplier();

        assertThat(keySupplier.getAdapterTag()).isEqualTo("ParameterizedTextElementAdapter");

        assertThat(keySupplier.getAdapter(context, adapterParameters)).isNotNull();

        TextElementKey key = new TextElementKey(font);
        assertThat(keySupplier.getKey(frameContext, model)).isEqualTo(key);
    }

    @Test
    public void testGetAdapterFromFactory() {
        AdapterFactory<ParameterizedTextElementAdapter, Element> textElementFactory =
                new AdapterFactory<>(context, adapterParameters, keySupplier);
        Element textElement = getBaseTextElementModel(null);

        ParameterizedTextElementAdapter textElementAdapter =
                textElementFactory.get(textElement, frameContext);

        // Verify we get the adapter from the KeySupplier, and we create but not bind it.
        assertThat(textElementAdapter).isSameInstanceAs(adapter);
        verify(adapter, never()).createAdapter(any(), any(), any());
        verify(adapter, never()).bindModel(any(), any(), any());
    }

    @Test
    public void testReleaseAndRecycling() {
        AdapterFactory<ParameterizedTextElementAdapter, Element> textElementFactory =
                new AdapterFactory<>(context, adapterParameters, keySupplier);
        Element textElement = getBaseTextElementModel(null);
        TextElementKey adapterKey =
                new TextElementKey(adapterParameters.defaultStyleProvider.getFont());
        when(adapter.getKey()).thenReturn(adapterKey);
        when(keySupplier.getKey(frameContext, textElement)).thenReturn(adapterKey);

        ParameterizedTextElementAdapter textElementAdapter =
                textElementFactory.get(textElement, frameContext);
        assertThat(textElementAdapter).isSameInstanceAs(adapter);
        textElementAdapter.createAdapter(textElement, frameContext);

        // Ensure that releasing in the factory releases the adapter.
        textElementFactory.release(textElementAdapter);
        verify(adapter).releaseAdapter();

        // Verify we get the same item when we create it again.
        TextElementAdapter textElementAdapter2 = textElementFactory.get(textElement, frameContext);
        assertThat(textElementAdapter2).isSameInstanceAs(adapter);
        assertThat(textElementAdapter2).isEqualTo(textElementAdapter);
        verify(adapter, never())
                .createAdapter(textElement, Element.getDefaultInstance(), frameContext);
        verify(adapter, never()).bindModel(any(), any(), any());

        // Verify we get a new item when we create another.
        TextElementAdapter textElementAdapter3 = textElementFactory.get(textElement, frameContext);
        assertThat(textElementAdapter3).isSameInstanceAs(adapter2);
        assertThat(textElementAdapter3).isNotSameInstanceAs(textElementAdapter);
    }

    private Element getBaseTextElementModel(/*@Nullable*/ StyleProvider styleProvider) {
        return Element.newBuilder().setTextElement(getBaseTextElement(styleProvider)).build();
    }

    private TextElement getBaseTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : adapterParameters.defaultStyleProvider;
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        TextElement.Builder textElement = TextElement.newBuilder();
        ParameterizedText text = ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT).build();
        textElement.setParameterizedText(text);
        return textElement.build();
    }
}

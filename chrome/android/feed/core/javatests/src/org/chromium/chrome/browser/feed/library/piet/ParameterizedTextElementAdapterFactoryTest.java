// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.TextElementAdapter.TextElementKey;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link TextElementAdapter} instance of the {@link AdapterFactory}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ParameterizedTextElementAdapterFactoryTest {
    private static final String TEXT_LINE_CONTENT = "Content";

    @Mock
    private FrameContext mFrameContext;
    @Mock
    ParameterizedTextElementAdapter.KeySupplier mKeySupplier;
    @Mock
    ParameterizedTextElementAdapter mAdapter;
    @Mock
    ParameterizedTextElementAdapter mAdapter2;

    private AdapterParameters mAdapterParameters;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mAdapterParameters = new AdapterParameters(null, null,
                new HostProviders(mock(AssetProvider.class), null, null, null), new FakeClock(),
                false, false);
        when(mKeySupplier.getAdapter(mContext, mAdapterParameters))
                .thenReturn(mAdapter)
                .thenReturn(mAdapter2);
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
        when(mFrameContext.makeStyleFor(style)).thenReturn(styleProvider);
        when(styleProvider.getFont()).thenReturn(font);
        ParameterizedTextElementAdapter.KeySupplier keySupplier =
                new ParameterizedTextElementAdapter.KeySupplier();

        assertThat(keySupplier.getAdapterTag()).isEqualTo("ParameterizedTextElementAdapter");

        assertThat(keySupplier.getAdapter(mContext, mAdapterParameters)).isNotNull();

        TextElementKey key = new TextElementKey(font);
        assertThat(keySupplier.getKey(mFrameContext, model)).isEqualTo(key);
    }

    @Test
    public void testGetAdapterFromFactory() {
        AdapterFactory<ParameterizedTextElementAdapter, Element> textElementFactory =
                new AdapterFactory<>(mContext, mAdapterParameters, mKeySupplier);
        Element textElement = getBaseTextElementModel(null);

        ParameterizedTextElementAdapter textElementAdapter =
                textElementFactory.get(textElement, mFrameContext);

        // Verify we get the adapter from the KeySupplier, and we create but not bind it.
        assertThat(textElementAdapter).isSameInstanceAs(mAdapter);
        verify(mAdapter, never()).createAdapter(any(), any(), any());
        verify(mAdapter, never()).bindModel(any(), any(), any());
    }

    @Test
    public void testReleaseAndRecycling() {
        AdapterFactory<ParameterizedTextElementAdapter, Element> textElementFactory =
                new AdapterFactory<>(mContext, mAdapterParameters, mKeySupplier);
        Element textElement = getBaseTextElementModel(null);
        TextElementKey adapterKey =
                new TextElementKey(mAdapterParameters.mDefaultStyleProvider.getFont());
        when(mAdapter.getKey()).thenReturn(adapterKey);
        when(mKeySupplier.getKey(mFrameContext, textElement)).thenReturn(adapterKey);

        ParameterizedTextElementAdapter textElementAdapter =
                textElementFactory.get(textElement, mFrameContext);
        assertThat(textElementAdapter).isSameInstanceAs(mAdapter);
        textElementAdapter.createAdapter(textElement, mFrameContext);

        // Ensure that releasing in the factory releases the adapter.
        textElementFactory.release(textElementAdapter);
        verify(mAdapter).releaseAdapter();

        // Verify we get the same item when we create it again.
        TextElementAdapter textElementAdapter2 = textElementFactory.get(textElement, mFrameContext);
        assertThat(textElementAdapter2).isSameInstanceAs(mAdapter);
        assertThat(textElementAdapter2).isEqualTo(textElementAdapter);
        verify(mAdapter, never())
                .createAdapter(textElement, Element.getDefaultInstance(), mFrameContext);
        verify(mAdapter, never()).bindModel(any(), any(), any());

        // Verify we get a new item when we create another.
        TextElementAdapter textElementAdapter3 = textElementFactory.get(textElement, mFrameContext);
        assertThat(textElementAdapter3).isSameInstanceAs(mAdapter2);
        assertThat(textElementAdapter3).isNotSameInstanceAs(textElementAdapter);
    }

    private Element getBaseTextElementModel(/*@Nullable*/ StyleProvider styleProvider) {
        return Element.newBuilder().setTextElement(getBaseTextElement(styleProvider)).build();
    }

    private TextElement getBaseTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : mAdapterParameters.mDefaultStyleProvider;
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        TextElement.Builder textElement = TextElement.newBuilder();
        ParameterizedText text = ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT).build();
        textElement.setParameterizedText(text);
        return textElement.build();
    }
}

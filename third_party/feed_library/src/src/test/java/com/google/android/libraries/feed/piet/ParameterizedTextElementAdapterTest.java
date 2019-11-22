// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.widget.TextView;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests of the {@link ParameterizedTextElementAdapter}; also tests base features of {@link
 * TextElementAdapter}.
 */
@RunWith(RobolectricTestRunner.class)
public class ParameterizedTextElementAdapterTest {
    private static final String TEXT_LINE_CONTENT = "Content";
    private static final String BINDING = "binding";
    private static final ParameterizedTextBindingRef DEFAULT_BINDING_REF =
            ParameterizedTextBindingRef.newBuilder().setBindingId(BINDING).build();

    @Mock
    private FrameContext frameContext;
    @Mock
    private StyleProvider mockStyleProvider;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;

    private AdapterParameters adapterParameters;

    private Context context;

    private ParameterizedTextElementAdapter adapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(assetProvider.isRtL()).thenReturn(false);
        when(mockStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());
        when(mockStyleProvider.getTextAlignment()).thenReturn(Gravity.START | Gravity.TOP);

        adapterParameters = new AdapterParameters(null, null, hostProviders,
                new ParameterizedTextEvaluator(new FakeClock()), null, null, new FakeClock());

        when(frameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(adapterParameters.defaultStyleProvider);

        adapter = new ParameterizedTextElementAdapter.KeySupplier().getAdapter(
                context, adapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(adapter).isNotNull();
    }

    @Test
    public void testBindModel_basic() {
        Element model = getBaseTextElement();
        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);
        assertThat(adapter.getView()).isNotNull();
        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo(TEXT_LINE_CONTENT);
    }

    @Test
    public void testBindModel_noContent() {
        adapter.createAdapter(getBaseTextElement(), frameContext);
        Element model = asElement(TextElement.getDefaultInstance());
        adapter.bindModel(model, frameContext);

        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEmpty();
        verify(frameContext)
                .reportMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        "TextElement missing ParameterizedText content; has CONTENT_NOT_SET");
    }

    @Test
    public void testBindModel_withBinding_someText() {
        ParameterizedText parameterizedText =
                ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT).build();
        BindingValue bindingValue =
                BindingValue.newBuilder().setParameterizedText(parameterizedText).build();
        when(frameContext.getParameterizedTextBindingValue(DEFAULT_BINDING_REF))
                .thenReturn(bindingValue);

        Element model = getBindingTextElement(null);
        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);
        assertThat(adapter.getView()).isNotNull();
        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo(TEXT_LINE_CONTENT);
    }

    @Test
    public void testBindModel_withBinding_noContent() {
        when(frameContext.getParameterizedTextBindingValue(DEFAULT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING).build());

        Element model = getBindingTextElement(null);
        adapter.createAdapter(model, frameContext);

        assertThatRunnable(() -> adapter.bindModel(model, frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Parameterized text binding binding had no content");
    }

    @Test
    public void testBindModel_withBinding_optionalAbsent() {
        Element model = getBindingTextElement(null /* StyleProvider*/);
        adapter.createAdapter(model, frameContext);
        Element modelOptionalBinding =
                asElement(TextElement.newBuilder()
                                  .setParameterizedTextBinding(
                                          DEFAULT_BINDING_REF.toBuilder().setIsOptional(true))
                                  .build());
        when(frameContext.getParameterizedTextBindingValue(
                     modelOptionalBinding.getTextElement().getParameterizedTextBinding()))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.bindModel(modelOptionalBinding, frameContext);

        assertThat(adapter.getView()).isNotNull();
        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEmpty();
    }

    @Test
    public void testBindModel_html() {
        Element model =
                asElement(TextElement.newBuilder()
                                  .setParameterizedText(
                                          ParameterizedText.newBuilder().setIsHtml(true).setText(
                                                  "<h1>HEADING!</h1>"))
                                  .build());
        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);
        assertThat(adapter.getView()).isNotNull();
        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo("HEADING!\n\n");
    }

    @Test
    public void testStyles_padding() {
        Element model = asElement(TextElement.getDefaultInstance());

        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(mockStyleProvider);

        when(mockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());

        adapter.createAdapter(model, frameContext);
        assertThat(adapter.getView()).isNotNull();
        verify(mockStyleProvider).applyElementStyles(adapter);
        TextView textView = adapter.getBaseView();
        assertThat(textView).isNotNull();
    }

    private Element getBindingTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : adapterParameters.defaultStyleProvider;
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);
        return asElement(
                TextElement.newBuilder().setParameterizedTextBinding(DEFAULT_BINDING_REF).build());
    }

    private Element getBaseTextElement() {
        return getBaseTextElement(null);
    }

    private Element getBaseTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : adapterParameters.defaultStyleProvider;
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        return asElement(TextElement.newBuilder()
                                 .setParameterizedText(
                                         ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT))
                                 .build());
    }

    private static Element asElement(TextElement textElement) {
        return Element.newBuilder().setTextElement(textElement).build();
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests of the {@link ParameterizedTextElementAdapter}; also tests base features of {@link
 * TextElementAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ParameterizedTextElementAdapterTest {
    private static final String TEXT_LINE_CONTENT = "Content";
    private static final String BINDING = "binding";
    private static final ParameterizedTextBindingRef DEFAULT_BINDING_REF =
            ParameterizedTextBindingRef.newBuilder().setBindingId(BINDING).build();

    @Mock
    private FrameContext mFrameContext;
    @Mock
    private StyleProvider mMockStyleProvider;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;

    private AdapterParameters mAdapterParameters;

    private Context mContext;

    private ParameterizedTextElementAdapter mAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mAssetProvider.isRtL()).thenReturn(false);
        when(mMockStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(mMockStyleProvider.getTextAlignment()).thenReturn(Gravity.START | Gravity.TOP);

        mAdapterParameters = new AdapterParameters(null, null, mHostProviders,
                new ParameterizedTextEvaluator(new FakeClock()), null, null, new FakeClock());

        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(mAdapterParameters.mDefaultStyleProvider);

        mAdapter = new ParameterizedTextElementAdapter.KeySupplier().getAdapter(
                mContext, mAdapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(mAdapter).isNotNull();
    }

    @Test
    public void testBindModel_basic() {
        Element model = getBaseTextElement();
        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);
        assertThat(mAdapter.getView()).isNotNull();
        TextView textView = mAdapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo(TEXT_LINE_CONTENT);
    }

    @Test
    public void testBindModel_noContent() {
        mAdapter.createAdapter(getBaseTextElement(), mFrameContext);
        Element model = asElement(TextElement.getDefaultInstance());
        mAdapter.bindModel(model, mFrameContext);

        TextView textView = mAdapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEmpty();
        verify(mFrameContext)
                .reportMessage(MessageType.ERROR, ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        "TextElement missing ParameterizedText content; has CONTENT_NOT_SET");
    }

    @Test
    public void testBindModel_withBinding_someText() {
        ParameterizedText parameterizedText =
                ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT).build();
        BindingValue bindingValue =
                BindingValue.newBuilder().setParameterizedText(parameterizedText).build();
        when(mFrameContext.getParameterizedTextBindingValue(DEFAULT_BINDING_REF))
                .thenReturn(bindingValue);

        Element model = getBindingTextElement(null);
        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);
        assertThat(mAdapter.getView()).isNotNull();
        TextView textView = mAdapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo(TEXT_LINE_CONTENT);
    }

    @Test
    public void testBindModel_withBinding_noContent() {
        when(mFrameContext.getParameterizedTextBindingValue(DEFAULT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setBindingId(BINDING).build());

        Element model = getBindingTextElement(null);
        mAdapter.createAdapter(model, mFrameContext);

        assertThatRunnable(() -> mAdapter.bindModel(model, mFrameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Parameterized text binding binding had no content");
    }

    @Test
    public void testBindModel_withBinding_optionalAbsent() {
        Element model = getBindingTextElement(null /* StyleProvider*/);
        mAdapter.createAdapter(model, mFrameContext);
        Element modelOptionalBinding =
                asElement(TextElement.newBuilder()
                                  .setParameterizedTextBinding(
                                          DEFAULT_BINDING_REF.toBuilder().setIsOptional(true))
                                  .build());
        when(mFrameContext.getParameterizedTextBindingValue(
                     modelOptionalBinding.getTextElement().getParameterizedTextBinding()))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.bindModel(modelOptionalBinding, mFrameContext);

        assertThat(mAdapter.getView()).isNotNull();
        TextView textView = mAdapter.getBaseView();
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
        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);
        assertThat(mAdapter.getView()).isNotNull();
        TextView textView = mAdapter.getBaseView();
        assertThat(textView).isNotNull();
        assertThat(textView.getText().toString()).isEqualTo("HEADING!\n\n");
    }

    @Test
    public void testStyles_padding() {
        Element model = asElement(TextElement.getDefaultInstance());

        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(mMockStyleProvider);

        when(mMockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());

        mAdapter.createAdapter(model, mFrameContext);
        assertThat(mAdapter.getView()).isNotNull();
        verify(mMockStyleProvider).applyElementStyles(mAdapter);
        TextView textView = mAdapter.getBaseView();
        assertThat(textView).isNotNull();
    }

    private Element getBindingTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : mAdapterParameters.mDefaultStyleProvider;
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);
        return asElement(
                TextElement.newBuilder().setParameterizedTextBinding(DEFAULT_BINDING_REF).build());
    }

    private Element getBaseTextElement() {
        return getBaseTextElement(null);
    }

    private Element getBaseTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : mAdapterParameters.mDefaultStyleProvider;
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        return asElement(TextElement.newBuilder()
                                 .setParameterizedText(
                                         ParameterizedText.newBuilder().setText(TEXT_LINE_CONTENT))
                                 .build());
    }

    private static Element asElement(TextElement textElement) {
        return Element.newBuilder().setTextElement(textElement).build();
    }
}

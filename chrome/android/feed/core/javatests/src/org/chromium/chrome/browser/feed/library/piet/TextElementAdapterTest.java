// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Build.VERSION_CODES;
import android.text.Layout;
import android.text.TextUtils.TruncateAt;
import android.view.Gravity;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.TextElementAdapter.TextElementKey;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider.GoogleSansTypeface;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.TextAlignmentHorizontal;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.TextAlignmentVertical;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Typeface.CommonTypeface;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link TextElementAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TextElementAdapterTest {
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private StyleProvider mMockStyleProvider;
    @Mock
    private HostProviders mMockHostProviders;
    @Mock
    private AssetProvider mMockAssetProvider;
    @Mock
    private TypefaceProvider mMockTypefaceProvider;

    private AdapterParameters mAdapterParameters;

    private Context mContext;

    private TextElementAdapter mAdapter;
    private int mEmptyTextElementLineHeight;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mMockHostProviders.getAssetProvider()).thenReturn(mMockAssetProvider);
        when(mMockAssetProvider.isRtL()).thenReturn(false);
        when(mMockStyleProvider.getRoundedCorners())
                .thenReturn(RoundedCorners.getDefaultInstance());
        when(mMockStyleProvider.getTextAlignment()).thenReturn(Gravity.START | Gravity.TOP);

        mAdapterParameters = new AdapterParameters(null, null, mMockHostProviders,
                new ParameterizedTextEvaluator(new FakeClock()), null, null, new FakeClock());

        TextElementAdapter adapterForEmptyElement =
                new TestTextElementAdapter(mContext, mAdapterParameters);
        // Get emptyTextElementHeight based on a text element with no content or styles set.
        Element textElement = getBaseTextElement();
        adapterForEmptyElement.createAdapter(textElement, mFrameContext);
        mEmptyTextElementLineHeight = adapterForEmptyElement.getBaseView().getLineHeight();

        mAdapter = new TestTextElementAdapter(mContext, mAdapterParameters);
    }

    @Test
    public void testHyphenationDisabled() {
        assertThat(mAdapter.getBaseView().getBreakStrategy())
                .isEqualTo(Layout.BREAK_STRATEGY_SIMPLE);
    }

    @Test
    public void testCreateAdapter_setsStyles() {
        Element textElement = getBaseTextElement(mMockStyleProvider);
        int color = Color.RED;
        int maxLines = 72;
        when(mMockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        when(mMockStyleProvider.getColor()).thenReturn(color);
        when(mMockStyleProvider.getMaxLines()).thenReturn(maxLines);

        mAdapter.createAdapter(textElement, mFrameContext);

        verify(mMockStyleProvider).applyElementStyles(mAdapter);
        assertThat(mAdapter.getBaseView().getMaxLines()).isEqualTo(maxLines);
        assertThat(mAdapter.getBaseView().getEllipsize()).isEqualTo(TruncateAt.END);
        assertThat(mAdapter.getBaseView().getCurrentTextColor()).isEqualTo(color);
    }

    @Test
    public void testSetFont_usesCommonFont() {
        Font font =
                Font.newBuilder()
                        .addTypeface(StylesProto.Typeface.newBuilder().setCommonTypeface(
                                CommonTypeface.PLATFORM_DEFAULT_MEDIUM))
                        .addTypeface(StylesProto.Typeface.newBuilder().setCustomTypeface("notused"))
                        .build();

        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);

        verify(mMockAssetProvider, never())
                .getTypeface(anyString(), anyBoolean(), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_callsHost() {
        Font font = Font.newBuilder()
                            .addTypeface(
                                    StylesProto.Typeface.newBuilder().setCustomTypeface("goodfont"))
                            .build();
        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);

        verify(mMockAssetProvider, atLeastOnce())
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_callsHostWithItalic() {
        Font font =
                Font.newBuilder()
                        .addTypeface(
                                StylesProto.Typeface.newBuilder().setCustomTypeface("goodfont"))
                        .addTypeface(StylesProto.Typeface.newBuilder().setCustomTypeface("badfont"))
                        .setItalic(true)
                        .build();
        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);

        verify(mMockAssetProvider, atLeastOnce())
                .getTypeface(eq("goodfont"), eq(true), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_callsHostWithFallback() {
        Font font =
                Font.newBuilder()
                        .addTypeface(StylesProto.Typeface.newBuilder().setCustomTypeface("badfont"))
                        .addTypeface(
                                StylesProto.Typeface.newBuilder().setCustomTypeface("goodfont"))
                        .build();
        TextElementKey key = new TextElementKey(font);
        // Consumer accepts null for badfont
        doAnswer(answer -> {
            Consumer<Typeface> typefaceConsumer = answer.getArgument(2);
            typefaceConsumer.accept(null);
            return null;
        })
                .when(mMockAssetProvider)
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        // Consumer accepts hosttypeface for goodfont
        Typeface hostTypeface = Typeface.create("host", Typeface.BOLD_ITALIC);
        doAnswer(answer -> {
            Consumer<Typeface> typefaceConsumer = answer.getArgument(2);
            typefaceConsumer.accept(hostTypeface);
            return null;
        })
                .when(mMockAssetProvider)
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);

        Typeface typeface = mAdapter.getBaseView().getTypeface();
        assertThat(typeface).isEqualTo(hostTypeface);
        InOrder inOrder = inOrder(mMockAssetProvider);
        inOrder.verify(mMockAssetProvider, atLeastOnce())
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        inOrder.verify(mMockAssetProvider, atLeastOnce())
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_hostReturnsNull() {
        Font font = Font.newBuilder()
                            .addTypeface(
                                    StylesProto.Typeface.newBuilder().setCustomTypeface("notvalid"))
                            .build();
        doAnswer(answer -> {
            Consumer<Typeface> typefaceConsumer = answer.getArgument(2);
            typefaceConsumer.accept(null);
            return null;
        })
                .when(mMockAssetProvider)
                .getTypeface(eq("notvalid"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);
        Typeface typeface = mAdapter.getBaseView().getTypeface();

        verify(mFrameContext)
                .reportMessage(MessageType.WARNING, ErrorCode.ERR_MISSING_FONTS,
                        "Could not load specified typefaces.");
        assertThat(typeface).isEqualTo(new TextView(mContext).getTypeface());
    }

    @Test
    public void testSetFont_callsHostForGoogleSans() {
        Font font = Font.newBuilder()
                            .addTypeface(StylesProto.Typeface.newBuilder().setCommonTypeface(
                                    CommonTypeface.GOOGLE_SANS_MEDIUM))
                            .build();
        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);

        verify(mMockAssetProvider, atLeastOnce())
                .getTypeface(eq(GoogleSansTypeface.GOOGLE_SANS_MEDIUM), eq(false),
                        ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_italics() {
        Font font = Font.newBuilder().setItalic(true).build();
        TextElementKey key = new TextElementKey(font);

        mAdapter.setValuesUsedInRecyclerKey(key, mFrameContext);
        Typeface typeface = mAdapter.getBaseView().getTypeface();
        // Typeface.isBold and Typeface.isItalic don't work properly in roboelectric.
        assertThat(typeface.getStyle() & Typeface.BOLD).isEqualTo(0);
        assertThat(typeface.getStyle() & Typeface.ITALIC).isGreaterThan(0);
    }

    @Test
    public void testGoogleSansEnumToStringDef() {
        assertThat(TextElementAdapter.googleSansEnumToStringDef(CommonTypeface.GOOGLE_SANS_REGULAR))
                .isEqualTo(GoogleSansTypeface.GOOGLE_SANS_REGULAR);
        assertThat(TextElementAdapter.googleSansEnumToStringDef(CommonTypeface.GOOGLE_SANS_MEDIUM))
                .isEqualTo(GoogleSansTypeface.GOOGLE_SANS_MEDIUM);
        assertThat(TextElementAdapter.googleSansEnumToStringDef(
                           CommonTypeface.PLATFORM_DEFAULT_MEDIUM))
                .isEqualTo(GoogleSansTypeface.UNDEFINED);
    }

    @Test
    public void testSetLineHeight() {
        int lineHeightToSetSp = 18;
        Style lineHeightStyle1 =
                Style.newBuilder()
                        .setFont(Font.newBuilder().setLineHeight(lineHeightToSetSp))
                        .build();
        StyleProvider styleProvider1 = new StyleProvider(lineHeightStyle1, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider1);

        mAdapter.createAdapter(textElement, mFrameContext);
        TextView textView = mAdapter.getBaseView();
        float actualLineHeightPx = textView.getLineHeight();
        int actualLineHeightSp =
                (int) LayoutUtils.pxToSp(actualLineHeightPx, textView.getContext());
        assertThat(actualLineHeightSp).isEqualTo(lineHeightToSetSp);
    }

    @Test
    public void testGetExtraLineHeight_roundDown() {
        // Extra height is 40.2px. This gets rounded down between the lines (to 40) and rounded up
        // for top and bottom padding (for 21 + 20 = 41).
        initializeAdapterWithExtraLineHeightPx(40.2f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(21);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Config(sdk = VERSION_CODES.KITKAT)
    @Test
    public void testGetExtraLineHeight_roundDown_kitkat() {
        // Extra height is 40.2px. In KitKat and lower, 40 pixels (the amount added between lines)
        // will have already been added to the bottom. To get to our desired value of 21 bottom
        // pixels, the actual bottom padding must be -19 (40 - 19 = 21).
        initializeAdapterWithExtraLineHeightPx(40.2f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(-19);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Test
    public void testGetExtraLineHeight_noRound() {
        // Extra height is 40px. 40 pixels will be added between each line, and that amount is split
        // (20 and 20) to be added to the top and bottom of the text element.
        initializeAdapterWithExtraLineHeightPx(40.0f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(20);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Config(sdk = VERSION_CODES.KITKAT)
    @Test
    public void testGetExtraLineHeight_noRound_kitkat() {
        // Extra height is 40px. In KitKat and lower, 40 pixels (the amount added between lines)
        // will have already been added to the bottom. To get to our desired value of 20 bottom
        // pixels, the actual bottom padding must be -20 (40 - 20 = 20).
        initializeAdapterWithExtraLineHeightPx(40.0f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(-20);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Test
    public void testGetExtraLineHeight_roundUp() {
        // Extra height is 40.8px. This gets rounded up between the lines (to 41) and rounded down
        // for top and bottom padding (for 20 + 20 = 40).
        initializeAdapterWithExtraLineHeightPx(40.8f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(41);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(20);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Config(sdk = VERSION_CODES.KITKAT)
    @Test
    public void testGetExtraLineHeight_roundUp_kitkat() {
        // Extra height is 40.8px. In KitKat and lower, 41 pixels (the amount added between lines)
        // will have already been added to the bottom. To get to our desired value of 20 bottom
        // pixels, the actual bottom padding must be -21 (41 - 21 = 20).
        initializeAdapterWithExtraLineHeightPx(40.8f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = mAdapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(41);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(-21);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    private void initializeAdapterWithExtraLineHeightPx(float lineHeightPx) {
        // Line height is specified in sp, so line height px = scaledDensity  x line height sp
        // These tests set display density because, in order to test the rounding behavior of
        // extraLineHeight, we need a lineHeight integer (in sp) that results in a decimal value in
        // px.
        int lineHeightSp = 10;
        float totalLineHeightPx = mEmptyTextElementLineHeight + lineHeightPx;
        mContext.getResources().getDisplayMetrics().scaledDensity =
                totalLineHeightPx / lineHeightSp;
        Style lineHeightStyle1 =
                Style.newBuilder().setFont(Font.newBuilder().setLineHeight(lineHeightSp)).build();
        StyleProvider styleProvider1 = new StyleProvider(lineHeightStyle1, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider1);
        mAdapter.createAdapter(textElement, mFrameContext);
    }

    @Test
    public void testBind_setsTextAlignment_horizontal() {
        Style style =
                Style.newBuilder()
                        .setTextAlignmentHorizontal(TextAlignmentHorizontal.TEXT_ALIGNMENT_CENTER)
                        .build();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);

        assertThat(mAdapter.getBaseView().getGravity())
                .isEqualTo(Gravity.CENTER_HORIZONTAL | Gravity.TOP);
    }

    @Test
    public void testBind_setsTextAlignment_vertical() {
        Style style = Style.newBuilder()
                              .setTextAlignmentVertical(TextAlignmentVertical.TEXT_ALIGNMENT_BOTTOM)
                              .build();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);

        assertThat(mAdapter.getBaseView().getGravity()).isEqualTo(Gravity.START | Gravity.BOTTOM);
    }

    @Test
    public void testBind_setsTextAlignment_both() {
        Style style =
                Style.newBuilder()
                        .setTextAlignmentHorizontal(TextAlignmentHorizontal.TEXT_ALIGNMENT_END)
                        .setTextAlignmentVertical(TextAlignmentVertical.TEXT_ALIGNMENT_MIDDLE)
                        .build();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);

        assertThat(mAdapter.getBaseView().getGravity())
                .isEqualTo(Gravity.END | Gravity.CENTER_VERTICAL);
    }

    @Test
    public void testBind_setsTextAlignment_default() {
        Style style = Style.getDefaultInstance();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        mAdapter.getBaseView().setGravity(Gravity.BOTTOM | Gravity.RIGHT);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);

        assertThat(mAdapter.getBaseView().getGravity()).isEqualTo(Gravity.START | Gravity.TOP);
    }

    @Test
    public void testBind_setsStylesOnlyIfBindingIsDefined() {
        int maxLines = 2;
        Style style = Style.newBuilder().setMaxLines(maxLines).build();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);
        assertThat(mAdapter.getBaseView().getMaxLines()).isEqualTo(maxLines);

        // Styles should not change on a re-bind
        mAdapter.unbindModel();
        StyleIdsStack otherStyle = StyleIdsStack.newBuilder().addStyleIds("ignored").build();
        textElement = getBaseTextElement().toBuilder().setStyleReferences(otherStyle).build();
        mAdapter.bindModel(textElement, mFrameContext);

        assertThat(mAdapter.getBaseView().getMaxLines()).isEqualTo(maxLines);
        verify(mFrameContext, never()).makeStyleFor(otherStyle);

        // Styles only change if new model has style bindings
        mAdapter.unbindModel();
        StyleIdsStack otherStyleWithBinding =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("prionailurus"))
                        .build();
        textElement =
                getBaseTextElement().toBuilder().setStyleReferences(otherStyleWithBinding).build();
        mAdapter.bindModel(textElement, mFrameContext);

        verify(mFrameContext).makeStyleFor(otherStyleWithBinding);
    }

    @Test
    public void bindWithUpdatedDensity_shouldUpdateLineHeight() {
        final int lineHeightInTextElement = 50;
        mContext.getResources().getDisplayMetrics().scaledDensity = 1;
        Style style = Style.newBuilder()
                              .setFont(Font.newBuilder().setLineHeight(lineHeightInTextElement))
                              .build();
        StyleProvider styleProvider = new StyleProvider(style, mMockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);

        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);
        assertThat(mAdapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement);

        mAdapter.unbindModel();
        // Change line height by changing the scale density
        mContext.getResources().getDisplayMetrics().scaledDensity = 2;
        mAdapter.bindModel(textElement, mFrameContext);
        // getLineHeight() still returns pixels. The number of pixels should have been updated to
        // reflect the new density.
        assertThat(mAdapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement * 2);

        mAdapter.unbindModel();
        // Make sure the line height is updated again when the scale density is changed back.
        mContext.getResources().getDisplayMetrics().scaledDensity = 1;
        mAdapter.bindModel(textElement, mFrameContext);
        assertThat(mAdapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement);
    }

    @Test
    public void testUnbind() {
        Element textElement = getBaseTextElement(null);
        mAdapter.createAdapter(textElement, mFrameContext);
        mAdapter.bindModel(textElement, mFrameContext);

        TextView adapterView = mAdapter.getBaseView();
        adapterView.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        adapterView.setText("OLD TEXT");

        mAdapter.unbindModel();

        assertThat(mAdapter.getBaseView()).isSameInstanceAs(adapterView);
        assertThat(adapterView.getTextAlignment()).isEqualTo(View.TEXT_ALIGNMENT_GRAVITY);
        assertThat(adapterView.getText().toString()).isEmpty();
    }

    @Test
    public void testGetStyles() {
        StyleIdsStack elementStyles = StyleIdsStack.newBuilder().addStyleIds("hair").build();
        when(mMockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        Element textElement = getBaseTextElement(mMockStyleProvider)
                                      .toBuilder()
                                      .setStyleReferences(elementStyles)
                                      .build();

        mAdapter.createAdapter(textElement, mFrameContext);

        assertThat(mAdapter.getElementStyleIdsStack()).isSameInstanceAs(elementStyles);
    }

    @Test
    public void testGetModelFromElement() {
        TextElement model =
                TextElement.newBuilder()
                        .setParameterizedText(ParameterizedText.newBuilder().setText("text"))
                        .build();

        Element elementWithModel =
                Element.newBuilder()
                        .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("spacer"))
                        .setTextElement(model)
                        .build();
        assertThat(mAdapter.getModelFromElement(elementWithModel))
                .isSameInstanceAs(elementWithModel);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> mAdapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing TextElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> mAdapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing TextElement");
    }

    private Element getBaseTextElement() {
        return getBaseTextElement(null);
    }

    private Element getBaseTextElement(/*@Nullable*/ StyleProvider styleProvider) {
        StyleProvider sp =
                styleProvider != null ? styleProvider : mAdapterParameters.mDefaultStyleProvider;
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        return Element.newBuilder().setTextElement(TextElement.getDefaultInstance()).build();
    }

    private static class TestTextElementAdapter extends TextElementAdapter {
        TestTextElementAdapter(Context mContext, AdapterParameters parameters) {
            super(mContext, parameters);
        }

        @Override
        void setTextOnView(FrameContext mFrameContext, TextElement textElement) {}

        @Override
        TextElementKey createKey(Font font) {
            return new TextElementKey(font);
        }
    }
}

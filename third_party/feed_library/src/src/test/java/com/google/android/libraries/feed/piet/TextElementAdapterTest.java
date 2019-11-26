// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
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

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.TextElementAdapter.TextElementKey;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.TypefaceProvider;
import com.google.android.libraries.feed.piet.host.TypefaceProvider.GoogleSansTypeface;
import com.google.search.now.ui.piet.BindingRefsProto.StyleBindingRef;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.StylesProto;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.StylesProto.TextAlignmentHorizontal;
import com.google.search.now.ui.piet.StylesProto.TextAlignmentVertical;
import com.google.search.now.ui.piet.StylesProto.Typeface.CommonTypeface;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests of the {@link TextElementAdapter}. */
@RunWith(RobolectricTestRunner.class)
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
        context = Robolectric.buildActivity(Activity.class).get();

        when(mockHostProviders.getAssetProvider()).thenReturn(mockAssetProvider);
        when(mockAssetProvider.isRtL()).thenReturn(false);
        when(mockStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());
        when(mockStyleProvider.getTextAlignment()).thenReturn(Gravity.START | Gravity.TOP);

        adapterParameters = new AdapterParameters(null, null, mockHostProviders,
                new ParameterizedTextEvaluator(new FakeClock()), null, null, new FakeClock());

        TextElementAdapter adapterForEmptyElement =
                new TestTextElementAdapter(context, adapterParameters);
        // Get emptyTextElementHeight based on a text element with no content or styles set.
        Element textElement = getBaseTextElement();
        adapterForEmptyElement.createAdapter(textElement, frameContext);
        emptyTextElementLineHeight = adapterForEmptyElement.getBaseView().getLineHeight();

        adapter = new TestTextElementAdapter(context, adapterParameters);
    }

    @Test
    public void testHyphenationDisabled() {
        assertThat(adapter.getBaseView().getBreakStrategy())
                .isEqualTo(Layout.BREAK_STRATEGY_SIMPLE);
    }

    @Test
    public void testCreateAdapter_setsStyles() {
        Element textElement = getBaseTextElement(mockStyleProvider);
        int color = Color.RED;
        int maxLines = 72;
        when(mockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        when(mockStyleProvider.getColor()).thenReturn(color);
        when(mockStyleProvider.getMaxLines()).thenReturn(maxLines);

        adapter.createAdapter(textElement, frameContext);

        verify(mockStyleProvider).applyElementStyles(adapter);
        assertThat(adapter.getBaseView().getMaxLines()).isEqualTo(maxLines);
        assertThat(adapter.getBaseView().getEllipsize()).isEqualTo(TruncateAt.END);
        assertThat(adapter.getBaseView().getCurrentTextColor()).isEqualTo(color);
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

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        verify(mockAssetProvider, never())
                .getTypeface(anyString(), anyBoolean(), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_callsHost() {
        Font font = Font.newBuilder()
                            .addTypeface(
                                    StylesProto.Typeface.newBuilder().setCustomTypeface("goodfont"))
                            .build();
        TextElementKey key = new TextElementKey(font);

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        verify(mockAssetProvider, atLeastOnce())
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

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        verify(mockAssetProvider, atLeastOnce())
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
                .when(mockAssetProvider)
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        // Consumer accepts hosttypeface for goodfont
        Typeface hostTypeface = Typeface.create("host", Typeface.BOLD_ITALIC);
        doAnswer(answer -> {
            Consumer<Typeface> typefaceConsumer = answer.getArgument(2);
            typefaceConsumer.accept(hostTypeface);
            return null;
        })
                .when(mockAssetProvider)
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        Typeface typeface = adapter.getBaseView().getTypeface();
        assertThat(typeface).isEqualTo(hostTypeface);
        InOrder inOrder = inOrder(mockAssetProvider);
        inOrder.verify(mockAssetProvider, atLeastOnce())
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        inOrder.verify(mockAssetProvider, atLeastOnce())
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
    }

    // Same test as above, but on JB. Catches bugs that might only appear on older versions, like
    // getting NPEs on getTypeface() (which is now fixed).
    @Config(sdk = VERSION_CODES.JELLY_BEAN)
    @Test
    public void testSetFont_callsHostWithFallback_JB() {
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
                .when(mockAssetProvider)
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        // Consumer accepts hosttypeface for goodfont
        Typeface hostTypeface = Typeface.create("host", Typeface.BOLD_ITALIC);
        doAnswer(answer -> {
            Consumer<Typeface> typefaceConsumer = answer.getArgument(2);
            typefaceConsumer.accept(hostTypeface);
            return null;
        })
                .when(mockAssetProvider)
                .getTypeface(eq("goodfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        Typeface typeface = adapter.getBaseView().getTypeface();
        assertThat(typeface).isEqualTo(hostTypeface);
        InOrder inOrder = inOrder(mockAssetProvider);
        inOrder.verify(mockAssetProvider, atLeastOnce())
                .getTypeface(eq("badfont"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        inOrder.verify(mockAssetProvider, atLeastOnce())
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
                .when(mockAssetProvider)
                .getTypeface(eq("notvalid"), eq(false), ArgumentMatchers.<Consumer<Typeface>>any());
        TextElementKey key = new TextElementKey(font);

        adapter.setValuesUsedInRecyclerKey(key, frameContext);
        Typeface typeface = adapter.getBaseView().getTypeface();

        verify(frameContext)
                .reportMessage(MessageType.WARNING, ErrorCode.ERR_MISSING_FONTS,
                        "Could not load specified typefaces.");
        assertThat(typeface).isEqualTo(new TextView(context).getTypeface());
    }

    @Test
    public void testSetFont_callsHostForGoogleSans() {
        Font font = Font.newBuilder()
                            .addTypeface(StylesProto.Typeface.newBuilder().setCommonTypeface(
                                    CommonTypeface.GOOGLE_SANS_MEDIUM))
                            .build();
        TextElementKey key = new TextElementKey(font);

        adapter.setValuesUsedInRecyclerKey(key, frameContext);

        verify(mockAssetProvider, atLeastOnce())
                .getTypeface(eq(GoogleSansTypeface.GOOGLE_SANS_MEDIUM), eq(false),
                        ArgumentMatchers.<Consumer<Typeface>>any());
    }

    @Test
    public void testSetFont_italics() {
        Font font = Font.newBuilder().setItalic(true).build();
        TextElementKey key = new TextElementKey(font);

        adapter.setValuesUsedInRecyclerKey(key, frameContext);
        Typeface typeface = adapter.getBaseView().getTypeface();
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
        StyleProvider styleProvider1 = new StyleProvider(lineHeightStyle1, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider1);

        adapter.createAdapter(textElement, frameContext);
        TextView textView = adapter.getBaseView();
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

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

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

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(-19);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Test
    public void testGetExtraLineHeight_noRound() {
        // Extra height is 40px. 40 pixels will be added between each line, and that amount is split
        // (20 and 20) to be added to the top and bottom of the text element.
        initializeAdapterWithExtraLineHeightPx(40.0f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

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

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

        assertThat(extraLineHeight.betweenLinesExtraPx()).isEqualTo(40);
        assertThat(extraLineHeight.bottomPaddingPx()).isEqualTo(-20);
        assertThat(extraLineHeight.topPaddingPx()).isEqualTo(20);
    }

    @Test
    public void testGetExtraLineHeight_roundUp() {
        // Extra height is 40.8px. This gets rounded up between the lines (to 41) and rounded down
        // for top and bottom padding (for 20 + 20 = 40).
        initializeAdapterWithExtraLineHeightPx(40.8f);

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

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

        TextElementAdapter.ExtraLineHeight extraLineHeight = adapter.getExtraLineHeight();

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
        float totalLineHeightPx = emptyTextElementLineHeight + lineHeightPx;
        context.getResources().getDisplayMetrics().scaledDensity = totalLineHeightPx / lineHeightSp;
        Style lineHeightStyle1 =
                Style.newBuilder().setFont(Font.newBuilder().setLineHeight(lineHeightSp)).build();
        StyleProvider styleProvider1 = new StyleProvider(lineHeightStyle1, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider1);
        adapter.createAdapter(textElement, frameContext);
    }

    @Test
    public void testBind_setsTextAlignment_horizontal() {
        Style style =
                Style.newBuilder()
                        .setTextAlignmentHorizontal(TextAlignmentHorizontal.TEXT_ALIGNMENT_CENTER)
                        .build();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);

        assertThat(adapter.getBaseView().getGravity())
                .isEqualTo(Gravity.CENTER_HORIZONTAL | Gravity.TOP);
    }

    @Test
    public void testBind_setsTextAlignment_vertical() {
        Style style = Style.newBuilder()
                              .setTextAlignmentVertical(TextAlignmentVertical.TEXT_ALIGNMENT_BOTTOM)
                              .build();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);

        assertThat(adapter.getBaseView().getGravity()).isEqualTo(Gravity.START | Gravity.BOTTOM);
    }

    @Test
    public void testBind_setsTextAlignment_both() {
        Style style =
                Style.newBuilder()
                        .setTextAlignmentHorizontal(TextAlignmentHorizontal.TEXT_ALIGNMENT_END)
                        .setTextAlignmentVertical(TextAlignmentVertical.TEXT_ALIGNMENT_MIDDLE)
                        .build();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);

        assertThat(adapter.getBaseView().getGravity())
                .isEqualTo(Gravity.END | Gravity.CENTER_VERTICAL);
    }

    @Test
    public void testBind_setsTextAlignment_default() {
        Style style = Style.getDefaultInstance();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        adapter.getBaseView().setGravity(Gravity.BOTTOM | Gravity.RIGHT);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);

        assertThat(adapter.getBaseView().getGravity()).isEqualTo(Gravity.START | Gravity.TOP);
    }

    @Test
    public void testBind_setsStylesOnlyIfBindingIsDefined() {
        int maxLines = 2;
        Style style = Style.newBuilder().setMaxLines(maxLines).build();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);
        assertThat(adapter.getBaseView().getMaxLines()).isEqualTo(maxLines);

        // Styles should not change on a re-bind
        adapter.unbindModel();
        StyleIdsStack otherStyle = StyleIdsStack.newBuilder().addStyleIds("ignored").build();
        textElement = getBaseTextElement().toBuilder().setStyleReferences(otherStyle).build();
        adapter.bindModel(textElement, frameContext);

        assertThat(adapter.getBaseView().getMaxLines()).isEqualTo(maxLines);
        verify(frameContext, never()).makeStyleFor(otherStyle);

        // Styles only change if new model has style bindings
        adapter.unbindModel();
        StyleIdsStack otherStyleWithBinding =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("prionailurus"))
                        .build();
        textElement =
                getBaseTextElement().toBuilder().setStyleReferences(otherStyleWithBinding).build();
        adapter.bindModel(textElement, frameContext);

        verify(frameContext).makeStyleFor(otherStyleWithBinding);
    }

    @Test
    public void bindWithUpdatedDensity_shouldUpdateLineHeight() {
        final int lineHeightInTextElement = 50;
        context.getResources().getDisplayMetrics().scaledDensity = 1;
        Style style = Style.newBuilder()
                              .setFont(Font.newBuilder().setLineHeight(lineHeightInTextElement))
                              .build();
        StyleProvider styleProvider = new StyleProvider(style, mockAssetProvider);
        Element textElement = getBaseTextElement(styleProvider);

        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);
        assertThat(adapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement);

        adapter.unbindModel();
        // Change line height by changing the scale density
        context.getResources().getDisplayMetrics().scaledDensity = 2;
        adapter.bindModel(textElement, frameContext);
        // getLineHeight() still returns pixels. The number of pixels should have been updated to
        // reflect the new density.
        assertThat(adapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement * 2);

        adapter.unbindModel();
        // Make sure the line height is updated again when the scale density is changed back.
        context.getResources().getDisplayMetrics().scaledDensity = 1;
        adapter.bindModel(textElement, frameContext);
        assertThat(adapter.getBaseView().getLineHeight()).isEqualTo(lineHeightInTextElement);
    }

    @Test
    public void testUnbind() {
        Element textElement = getBaseTextElement(null);
        adapter.createAdapter(textElement, frameContext);
        adapter.bindModel(textElement, frameContext);

        TextView adapterView = adapter.getBaseView();
        adapterView.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        adapterView.setText("OLD TEXT");

        adapter.unbindModel();

        assertThat(adapter.getBaseView()).isSameInstanceAs(adapterView);
        assertThat(adapterView.getTextAlignment()).isEqualTo(View.TEXT_ALIGNMENT_GRAVITY);
        assertThat(adapterView.getText().toString()).isEmpty();
    }

    @Test
    public void testGetStyles() {
        StyleIdsStack elementStyles = StyleIdsStack.newBuilder().addStyleIds("hair").build();
        when(mockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        Element textElement = getBaseTextElement(mockStyleProvider)
                                      .toBuilder()
                                      .setStyleReferences(elementStyles)
                                      .build();

        adapter.createAdapter(textElement, frameContext);

        assertThat(adapter.getElementStyleIdsStack()).isSameInstanceAs(elementStyles);
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
        assertThat(adapter.getModelFromElement(elementWithModel))
                .isSameInstanceAs(elementWithModel);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> adapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing TextElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> adapter.getModelFromElement(emptyElement))
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
                styleProvider != null ? styleProvider : adapterParameters.defaultStyleProvider;
        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(sp);

        return Element.newBuilder().setTextElement(TextElement.getDefaultInstance()).build();
    }

    private static class TestTextElementAdapter extends TextElementAdapter {
        TestTextElementAdapter(Context context, AdapterParameters parameters) {
            super(context, parameters);
        }

        @Override
        void setTextOnView(FrameContext frameContext, TextElement textElement) {}

        @Override
        TextElementKey createKey(Font font) {
            return new TextElementKey(font);
        }
    }
}

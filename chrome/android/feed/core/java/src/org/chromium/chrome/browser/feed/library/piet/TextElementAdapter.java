// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.support.annotation.VisibleForTesting;
import android.support.v4.widget.TextViewCompat;
import android.text.Layout;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.AdapterKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider.GoogleSansTypeface;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Typeface.CommonTypeface;

import java.util.List;

/**
 * Base {@link ElementAdapter} to extend to manage {@code ChunkedText} and {@code ParameterizedText}
 * elements.
 */
abstract class TextElementAdapter extends ElementAdapter<TextView, Element> {
    private ExtraLineHeight mExtraLineHeight = ExtraLineHeight.builder().build();

    TextElementAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, createView(context));
    }

    @Override
    protected Element getModelFromElement(Element baseElement) {
        if (!baseElement.hasTextElement()) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing TextElement; has %s", baseElement.getElementsCase()));
        }
        return baseElement;
    }

    @Override
    void onCreateAdapter(Element textLine, Element baseElement, FrameContext frameContext) {
        if (getKey() == null) {
            TextElementKey key = createKey(getElementStyle().getFont());
            setKey(key);
            setValuesUsedInRecyclerKey(key, frameContext);
        }

        // Setup the layout of the text lines, including all properties not in the recycler key.
        updateTextStyle();
    }

    private void updateTextStyle() {
        TextView textView = getBaseView();
        StyleProvider textStyle = getElementStyle();
        textView.setTextColor(textStyle.getColor());

        if (textStyle.getFont().hasLineHeight()) {
            textView.setIncludeFontPadding(false);
            ExtraLineHeight extraLineHeight = getExtraLineHeight();
            textView.setLineSpacing(
                    /* add= */ extraLineHeight.betweenLinesExtraPx(), /* mult= */ 1.0f);
        }
        setLetterSpacing(textView, textStyle);
        if (textStyle.getMaxLines() > 0) {
            textView.setMaxLines(textStyle.getMaxLines());
            textView.setEllipsize(TextUtils.TruncateAt.END);
        } else {
            // MAX_VALUE is the value used in the Android implementation for the default
            textView.setMaxLines(Integer.MAX_VALUE);
        }

        getBaseView().setGravity(textStyle.getTextAlignment());
    }

    private void setLetterSpacing(TextView textView, StyleProvider textStyle) {
        if (!textStyle.getFont().hasLetterSpacingDp()) {
            return;
        }
        float textSize;
        if (textStyle.getFont().hasSize()) {
            textSize = textStyle.getFont().getSize();
        } else {
            textSize = LayoutUtils.pxToSp(textView.getTextSize(), textView.getContext());
        }
        float letterSpacingDp = textStyle.getFont().getLetterSpacingDp();
        float letterSpacingEm = letterSpacingDp / textSize;
        if (VERSION.SDK_INT >= VERSION_CODES.LOLLIPOP) {
            textView.setLetterSpacing(letterSpacingEm);
        } else {
            // Letter spacing wasn't supported before L. We substitute SetTextScaleX, which actually
            // stretches the letters, rather than just adding space between them. It won't look
            // exactly the same, but we can use it to get close to the same width for a set of
            // characters.
            float extraLetterSpaceDp = letterSpacingEm * textSize;
            // It can vary by font and character, but typically letter width is about half of
            // height.
            float approximateLetterwidth = textSize / 2;
            float textScale =
                    (approximateLetterwidth + extraLetterSpaceDp) / approximateLetterwidth;
            textView.setTextScaleX(textScale);
        }
    }

    @Override
    void onBindModel(Element textLine, Element baseElement, FrameContext frameContext) {
        // Set the initial state for the TextView
        // No bindings found, so use the inlined value (or empty if not set)
        setTextOnView(frameContext, textLine.getTextElement());

        if (getElementStyleIdsStack().hasStyleBinding()) {
            updateTextStyle();
        } else if (baseFontHeightChanged()) {
            updateTextStyle();
            getElementStyle().applyElementStyles(this);
        }
    }

    abstract void setTextOnView(FrameContext frameContext, TextElement textElement);

    @Override
    void onUnbindModel() {
        TextView textView = getBaseView();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            textView.setTextAlignment(View.TEXT_ALIGNMENT_GRAVITY);
        }
        textView.setText("");
    }

    private float calculateCurrentAndExpectedLineHeightDifference() {
        TextView textView = getBaseView();
        StyleProvider textStyle = getElementStyle();

        float lineHeightGoalSp = textStyle.getFont().getLineHeight();
        float lineHeightGoalPx = LayoutUtils.spToPx(lineHeightGoalSp, textView.getContext());
        float currentHeight = textView.getLineHeight();

        return (lineHeightGoalPx - currentHeight);
    }

    /**
     * Returns a line height object which contains the number of pixels that need to be added
     * between each line, as well as the number of pixels that need to be added to the top and
     * bottom padding of the element in order to match css line height behavior.
     */
    ExtraLineHeight getExtraLineHeight() {
        Font font = getElementStyle().getFont();

        if (!font.hasLineHeight()) {
            return mExtraLineHeight;
        }

        int totalExtraPadding = 0;
        int extraLineHeightBetweenLines = 0;
        if (font.hasLineHeight()) {
            float changeInLineHeight = calculateCurrentAndExpectedLineHeightDifference();
            if (changeInLineHeight == 0) {
                return mExtraLineHeight;
            }

            float extraLineHeightBetweenLinesFloat =
                    mExtraLineHeight.betweenLinesExtraPx() + changeInLineHeight;
            extraLineHeightBetweenLines = Math.round(extraLineHeightBetweenLinesFloat);

            // Adjust the rounding for the extra top and bottom padding, to make the total height of
            // the text element a little more exact.
            totalExtraPadding = adjustRounding(extraLineHeightBetweenLinesFloat);
        }
        int extraPaddingForLineHeightTop = totalExtraPadding / 2;
        int extraPaddingForLineHeightBottom = totalExtraPadding - extraPaddingForLineHeightTop;
        // In API version 21 (Lollipop), the implementation of lineSpacingMultiplier() changed to
        // add no extra space beneath a block of text. Before API 21, we need to subtract the extra
        // padding (so that only half the padding is on the bottom). That means
        // extraPaddingForLineHeightBottom needs to be negative.
        if (VERSION.SDK_INT < VERSION_CODES.LOLLIPOP) {
            extraPaddingForLineHeightBottom =
                    -(extraLineHeightBetweenLines - extraPaddingForLineHeightBottom);
        }

        mExtraLineHeight = ExtraLineHeight.builder()
                                   .setTopPaddingPx(extraPaddingForLineHeightTop)
                                   .setBottomPaddingPx(extraPaddingForLineHeightBottom)
                                   .setBetweenLinesExtraPx(extraLineHeightBetweenLines)
                                   .build();

        return mExtraLineHeight;
    }

    private boolean baseFontHeightChanged() {
        // Check if we've already calculated the extra line height and there is a significant
        // difference between the current and expected line heights.
        if (mExtraLineHeight.betweenLinesExtraPx() == 0) {
            return false;
        }
        float lineHeightDifference = calculateCurrentAndExpectedLineHeightDifference();
        if (Math.round(lineHeightDifference) == 0) {
            return false;
        }
        return true;
    }

    /**
     * Rounds the float value away from the nearest integer, i.e. 4.75 rounds to 4, and 7.2 rounds
     * to 8.
     */
    private int adjustRounding(float floatValueToRound) {
        int intWithRegularRounding = Math.round(floatValueToRound);
        // If the regular rounding rounded up, round down with adjusted rounding.
        if (floatValueToRound - (float) intWithRegularRounding < 0) {
            return intWithRegularRounding - 1;
        }
        // If the regular rounding rounded down, round up with adjusted rounding.
        if (floatValueToRound - (float) intWithRegularRounding > 0) {
            return intWithRegularRounding + 1;
        }
        return intWithRegularRounding;
    }

    @VisibleForTesting
    // LINT.IfChange
    void setValuesUsedInRecyclerKey(TextElementKey fontKey, FrameContext frameContext) {
        TextView textView = getBaseView();
        textView.setTextSize(fontKey.getSize());
        if (!fontKey.mTypefaces.isEmpty()) {
            FontDetails fontDetails =
                    new FontDetails(fontKey.mTypefaces, fontKey.isItalic(), frameContext);
            loadFont(textView, fontDetails);
        } else {
            makeFontItalic(textView, fontKey.isItalic());
        }
    }

    private void loadFont(TextView textView, FontDetails fontDetails) {
        StylesProto.Typeface typeface = fontDetails.getTypefaceToLoad();
        if (typeface == null) {
            fontDetails.getFrameContext().reportMessage(MessageType.WARNING,
                    ErrorCode.ERR_MISSING_FONTS, "Could not load specified typefaces.");
            // We didn't load a typeface, but we can at least respect italicization.
            makeFontItalic(textView, fontDetails.isItalic());
            return;
        }
        switch (typeface.getTypefaceSpecifierCase()) {
            case COMMON_TYPEFACE:
                loadCommonTypeface(typeface.getCommonTypeface(), fontDetails, textView);
                break;
            case CUSTOM_TYPEFACE:
                loadCustomTypeface(typeface.getCustomTypeface(), fontDetails, textView);
                break;
            default:
                // do nothing
        }
    }

    /** Load one of the typefaces from the {@link CommonTypeface} enum. */
    private void loadCommonTypeface(
            CommonTypeface commonTypeface, FontDetails fontDetails, TextView textView) {
        switch (commonTypeface) {
            case PLATFORM_DEFAULT_LIGHT:
                TextViewCompat.setTextAppearance(textView, R.style.gm_font_weight_light);
                break;
            case PLATFORM_DEFAULT_REGULAR:
                TextViewCompat.setTextAppearance(textView, R.style.gm_font_weight_regular);
                break;
            case PLATFORM_DEFAULT_MEDIUM:
                TextViewCompat.setTextAppearance(textView, R.style.gm_font_weight_medium);
                break;
            case GOOGLE_SANS_MEDIUM:
            case GOOGLE_SANS_REGULAR:
                loadCustomTypeface(
                        googleSansEnumToStringDef(commonTypeface), fontDetails, textView);
                // The host should take care of italicization for custom fonts, so return here.
                return;
            default:
                // Unrecognized common typeface. Try to load the next typeface from fontDetails.
                // This should never happen.
                fontDetails.currentTypefaceFailedToLoad();
                loadFont(textView, fontDetails);
                return;
        }
        makeFontItalic(textView, fontDetails.isItalic());
    }

    /** Ask the host to load a typeface by string identifier. */
    private void loadCustomTypeface(
            String customTypefaceName, FontDetails fontDetails, TextView textView) {
        TypefaceCallback typefaceCallback = new TypefaceCallback(textView, fontDetails);
        getParameters().mHostProviders.getAssetProvider().getTypeface(
                customTypefaceName, fontDetails.isItalic(), typefaceCallback);
    }

    /**
     * Conversion method to avoid version skew issues if we would ever change the enum names in the
     * CommonTypeface proto, so we don't need to change all the hosts or old clients.
     */
    @VisibleForTesting
    @GoogleSansTypeface
    static String googleSansEnumToStringDef(CommonTypeface googleSansType) {
        switch (googleSansType) {
            case GOOGLE_SANS_MEDIUM:
                return GoogleSansTypeface.GOOGLE_SANS_MEDIUM;
            case GOOGLE_SANS_REGULAR:
                return GoogleSansTypeface.GOOGLE_SANS_REGULAR;
            default:
                return GoogleSansTypeface.UNDEFINED;
        }
    }

    private static void makeFontItalic(TextView textView, boolean isItalic) {
        if (isItalic) {
            textView.setTypeface(textView.getTypeface(), Typeface.ITALIC);
        } else {
            textView.setTypeface(Typeface.create(textView.getTypeface(), Typeface.NORMAL));
        }
    }

    private static TextView createView(Context context) {
        TextView view = new TextView(context);
        if (Build.VERSION.SDK_INT >= VERSION_CODES.M) {
            view.setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE);
        }
        return view;
    }

    TextElementKey createKey(Font font) {
        return new TextElementKey(font);
    }

    abstract static class TextElementKeySupplier<A extends TextElementAdapter>
            implements AdapterKeySupplier<A, Element> {
        @Override
        public TextElementKey getKey(FrameContext frameContext, Element model) {
            StyleProvider styleProvider = frameContext.makeStyleFor(model.getStyleReferences());
            return new TextElementKey(styleProvider.getFont());
        }
    }

    /** We will Key TextViews off of Font Size, Typefaces and Italics. */
    // LINT.IfChange
    static class TextElementKey extends RecyclerKey {
        private final int mSize;
        private final boolean mItalic;
        private final List<StylesProto.Typeface> mTypefaces;

        TextElementKey(Font font) {
            mSize = font.getSize();
            mItalic = font.getItalic();
            mTypefaces = font.getTypefaceList();
        }

        public int getSize() {
            return mSize;
        }

        public boolean isItalic() {
            return mItalic;
        }

        @Override
        public int hashCode() {
            // Can't use Objects.hash() as it is only available in KK+ and can't use Guava's impl
            // either.
            int result = mSize;
            result = 31 * result + (mItalic ? 1 : 0);
            result = 31 * result + mTypefaces.hashCode();
            return result;
        }

        @Override
        public boolean equals(/*@Nullable*/ Object obj) {
            if (obj == this) {
                return true;
            }

            if (obj == null) {
                return false;
            }

            if (!(obj instanceof TextElementKey)) {
                return false;
            }

            TextElementKey key = (TextElementKey) obj;
            return key.mSize == mSize && key.mItalic == mItalic
                    && mTypefaces.equals(key.mTypefaces);
        }
    }
    // LINT.ThenChange

    static class ExtraLineHeight {
        private final int mTopPaddingPx;
        private final int mBottomPaddingPx;
        private final int mBetweenLinesExtraPx;

        int topPaddingPx() {
            return mTopPaddingPx;
        }

        int bottomPaddingPx() {
            return mBottomPaddingPx;
        }

        int betweenLinesExtraPx() {
            return mBetweenLinesExtraPx;
        }

        private ExtraLineHeight(Builder builder) {
            this.mTopPaddingPx = builder.mTopPaddingPx;
            this.mBottomPaddingPx = builder.mBottomPaddingPx;
            this.mBetweenLinesExtraPx = builder.mBetweenLinesExtraPx;
        }

        static Builder builder() {
            return new ExtraLineHeight.Builder();
        }

        static class Builder {
            private int mTopPaddingPx;
            private int mBottomPaddingPx;
            private int mBetweenLinesExtraPx;

            Builder setTopPaddingPx(int value) {
                mTopPaddingPx = value;
                return this;
            }

            Builder setBottomPaddingPx(int value) {
                mBottomPaddingPx = value;
                return this;
            }

            Builder setBetweenLinesExtraPx(int value) {
                mBetweenLinesExtraPx = value;
                return this;
            }

            ExtraLineHeight build() {
                return new ExtraLineHeight(this);
            }
        }
    }

    static class FontDetails {
        private int mFontIndexToLoad;
        private final List<StylesProto.Typeface> mTypefaceList;
        private final boolean mIsItalic;
        private final FrameContext mFrameContextForErrors;

        FontDetails(List<StylesProto.Typeface> typefaceList, boolean isItalic,
                FrameContext frameContext) {
            this.mTypefaceList = typefaceList;
            this.mIsItalic = isItalic;
            this.mFrameContextForErrors = frameContext;
        }

        FrameContext getFrameContext() {
            return mFrameContextForErrors;
        }

        StylesProto./*@Nullable*/ Typeface getTypefaceToLoad() {
            if (mTypefaceList.size() <= mFontIndexToLoad) {
                return null;
            }
            return mTypefaceList.get(mFontIndexToLoad);
        }

        void currentTypefaceFailedToLoad() {
            mFontIndexToLoad++;
        }

        boolean isItalic() {
            return mIsItalic;
        }
    }

    class TypefaceCallback implements Consumer</*@Nullable*/ Typeface> {
        private final TextView mTextView;
        private final FontDetails mFontDetails;

        TypefaceCallback(TextView textView, FontDetails fontDetails) {
            this.mTextView = textView;
            this.mFontDetails = fontDetails;
        }

        @Override
        public void accept(/*@Nullable*/ Typeface typeface) {
            if (typeface == null) {
                mFontDetails.currentTypefaceFailedToLoad();
                loadFont(mTextView, mFontDetails);
                return;
            }
            if (mTextView.getTypeface() == null || !mTextView.getTypeface().equals(typeface)) {
                mTextView.setTypeface(typeface);
            }
        }
    }
}

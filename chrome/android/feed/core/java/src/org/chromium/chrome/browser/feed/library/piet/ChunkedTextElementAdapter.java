// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_BINDING_VALUE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.text.Layout;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.StyleSpan;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ImageBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.Chunk;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ChunkedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.StyledImageChunk;

import java.util.HashSet;
import java.util.Set;

/** An {@link ElementAdapter} which manages {@code ChunkedText} elements. */
class ChunkedTextElementAdapter extends TextElementAdapter {
    private static final String TAG = "ChunkedTextElementAdapter";

    // We only use a LayerDrawable so we can switch out the Drawable; we only want one layer.
    @VisibleForTesting
    static final int SINGLE_LAYER_ID = 0;

    private final Set<ImageSpanDrawableCallback> mLoadingImages = new HashSet<>();

    ChunkedTextElementAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters);
    }

    @Override
    void setTextOnView(FrameContext frameContext, TextElement textLine) {
        ChunkedText chunkedText;
        switch (textLine.getContentCase()) {
            case CHUNKED_TEXT:
                chunkedText = textLine.getChunkedText();
                break;
            case CHUNKED_TEXT_BINDING:
                BindingValue binding =
                        frameContext.getChunkedTextBindingValue(textLine.getChunkedTextBinding());
                if (!binding.hasChunkedText()) {
                    if (textLine.getChunkedTextBinding().getIsOptional()) {
                        setVisibilityOnView(Visibility.GONE);
                        return;
                    } else {
                        throw new PietFatalException(ERR_MISSING_BINDING_VALUE,
                                frameContext.reportMessage(MessageType.ERROR,
                                        ERR_MISSING_BINDING_VALUE,
                                        String.format("Chunked text binding %s had no content",
                                                binding.getBindingId())));
                    }
                }
                chunkedText = binding.getChunkedText();
                break;
            default:
                throw new PietFatalException(ERR_MISSING_OR_UNHANDLED_CONTENT,
                        frameContext.reportMessage(MessageType.ERROR,
                                ERR_MISSING_OR_UNHANDLED_CONTENT,
                                String.format("Unhandled type of TextElement; had content %s",
                                        textLine.getContentCase())));
        }
        bindChunkedText(chunkedText, frameContext);
    }

    private void bindChunkedText(ChunkedText chunkedText, FrameContext frameContext) {
        TextView textView = getBaseView();
        SpannableStringBuilder spannable = new SpannableStringBuilder();
        boolean hasTouchListener = false;
        for (Chunk chunk : chunkedText.getChunksList()) {
            int chunkStart = spannable.length();
            switch (chunk.getContentCase()) {
                case TEXT_CHUNK:
                    addTextChunk(frameContext, spannable, chunk);
                    break;
                case IMAGE_CHUNK:
                    addImageChunk(frameContext, textView, spannable, chunk);
                    break;

                default:
                    throw new PietFatalException(ERR_MISSING_OR_UNHANDLED_CONTENT,
                            String.format("Unhandled type of ChunkedText Chunk; had content %s",
                                    chunk.getContentCase()));
            }
            int chunkEnd = spannable.length();
            switch (chunk.getActionsDataCase()) {
                case ACTIONS:
                    setChunkActions(chunk.getActions(), spannable, textView, frameContext,
                            chunkStart, chunkEnd, hasTouchListener);
                    hasTouchListener = true;
                    break;
                case ACTIONS_BINDING:
                    Actions boundActions =
                            frameContext.getActionsFromBinding(chunk.getActionsBinding());
                    if (boundActions == null) {
                        break;
                    }
                    setChunkActions(boundActions, spannable, textView, frameContext, chunkStart,
                            chunkEnd, hasTouchListener);
                    hasTouchListener = true;
                    break;
                default:
                    // No actions
            }
        }
        textView.setText(spannable);
    }

    private void setChunkActions(Actions actions, SpannableStringBuilder spannable,
            TextView textView, FrameContext frameContext, int chunkStart, int chunkEnd,
            boolean hasTouchListener) {
        // TODO: Also support long click actions.
        if (actions.hasOnClickAction()) {
            spannable.setSpan(new ActionsClickableSpan(actions.getOnClickAction(),
                                      frameContext.getActionHandler(), frameContext.getFrame()),
                    chunkStart, chunkEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            if (!hasTouchListener) {
                textView.setOnTouchListener(new ChunkedTextTouchListener(spannable));
            }
        }
    }

    @VisibleForTesting
    void addTextChunk(FrameContext frameContext, SpannableStringBuilder spannable, Chunk chunk) {
        int start = spannable.length();
        ParameterizedText parameterizedText;
        switch (chunk.getTextChunk().getContentCase()) {
            case PARAMETERIZED_TEXT:
                parameterizedText = chunk.getTextChunk().getParameterizedText();
                break;
            case PARAMETERIZED_TEXT_BINDING:
                BindingValue textBindingValue = frameContext.getParameterizedTextBindingValue(
                        chunk.getTextChunk().getParameterizedTextBinding());
                parameterizedText = textBindingValue.getParameterizedText();
                break;
            default:
                frameContext.reportMessage(MessageType.ERROR, ERR_MISSING_OR_UNHANDLED_CONTENT,
                        String.format("StyledTextChunk missing ParameterizedText content; has %s",
                                chunk.getTextChunk().getContentCase()));
                parameterizedText = ParameterizedText.getDefaultInstance();
        }
        StyleProvider chunkStyle =
                frameContext.makeStyleFor(chunk.getTextChunk().getStyleReferences());
        if (chunkStyle.getMargins().hasStart()) {
            addMargin(spannable, chunkStyle.getMargins().getStart());
        }

        CharSequence evaluatedText = getParameters().mTemplatedStringEvaluator.evaluate(
                getParameters().mHostProviders.getAssetProvider(), parameterizedText);
        spannable.append(evaluatedText);
        if (chunkStyle.hasColor()) {
            spannable.setSpan(new ForegroundColorSpan(chunkStyle.getColor()), start,
                    spannable.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (chunkStyle.getFont().getItalic()) {
            spannable.setSpan(new StyleSpan(Typeface.ITALIC), start, spannable.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (chunkStyle.getFont().hasSize()) {
            spannable.setSpan(new AbsoluteSizeSpan((int) LayoutUtils.spToPx(
                                      chunkStyle.getFont().getSize(), getContext())),
                    start, spannable.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (chunkStyle.getMaxLines() > 0) {
            Logger.e(TAG,
                    frameContext.reportMessage(MessageType.WARNING,
                            ErrorCode.ERR_UNSUPPORTED_FEATURE,
                            String.format("Ignoring max lines parameter set to '%s';"
                                            + " not supported on chunks.",
                                    chunkStyle.getMaxLines())));
        }

        if (chunkStyle.getMargins().hasEnd()) {
            addMargin(spannable, chunkStyle.getMargins().getEnd());
        }
    }

    @VisibleForTesting
    void addImageChunk(FrameContext frameContext, TextView textView,
            SpannableStringBuilder spannable, Chunk chunk) {
        StyledImageChunk imageChunk = chunk.getImageChunk();
        Image image;
        switch (imageChunk.getContentCase()) {
            case IMAGE:
                image = imageChunk.getImage();
                break;
            case IMAGE_BINDING:
                ImageBindingRef imageBinding = imageChunk.getImageBinding();
                BindingValue imageBindingValue = frameContext.getImageBindingValue(imageBinding);
                if (!imageBindingValue.hasImage()) {
                    if (!imageBinding.getIsOptional()) {
                        frameContext.reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                                String.format("No image found for binding id: %s",
                                        imageBinding.getBindingId()));
                    }
                    return;
                }
                image = imageBindingValue.getImage();
                break;
            default:
                frameContext.reportMessage(MessageType.ERROR, ERR_MISSING_OR_UNHANDLED_CONTENT,
                        String.format("StyledImageChunk missing Image content; has %s",
                                imageChunk.getContentCase()));
                return;
        }

        image = frameContext.filterImageSourcesByMediaQueryCondition(image);

        StyleProvider chunkStyle = frameContext.makeStyleFor(imageChunk.getStyleReferences());

        // Set a placeholder empty image
        Drawable placeholder = null;
        if (chunkStyle.hasPreLoadFill()) {
            placeholder = chunkStyle.createPreLoadFill();
        }
        if (placeholder == null) {
            placeholder = new ColorDrawable(Color.TRANSPARENT);
        }
        LayerDrawable wrapper = new LayerDrawable(new Drawable[] {placeholder});
        wrapper.setId(0, SINGLE_LAYER_ID);
        setBounds(wrapper, chunkStyle, textView);
        ImageSpan imageSpan = new ImageSpan(wrapper, ImageSpan.ALIGN_BASELINE);

        if (chunkStyle.getMargins().hasStart()) {
            addMargin(spannable, chunkStyle.getMargins().getStart());
        }

        spannable.append(" "); // dummy space to overwrite
        spannable.setSpan(imageSpan, spannable.length() - 1, spannable.length(),
                Spannable.SPAN_INCLUSIVE_EXCLUSIVE);

        if (chunkStyle.getMargins().hasEnd()) {
            addMargin(spannable, chunkStyle.getMargins().getEnd());
        }

        // Start asynchronously loading the real image.
        Integer overlayColor = chunkStyle.hasColor() ? chunkStyle.getColor() : null;
        ImageSpanDrawableCallback imageSpanLoader =
                new ImageSpanDrawableCallback(wrapper, chunkStyle, overlayColor, textView);
        mLoadingImages.add(imageSpanLoader);
        int styleWidth = styleToImageDim(chunkStyle.getWidthSpecPx(getContext()));
        int styleHeight = styleToImageDim(chunkStyle.getHeightSpecPx(getContext()));
        getParameters().mHostProviders.getAssetProvider().getImage(
                image, styleWidth, styleHeight, imageSpanLoader);
    }

    @Override
    void onUnbindModel() {
        for (ImageSpanDrawableCallback imageLoader : mLoadingImages) {
            imageLoader.cancelCallback();
        }
        mLoadingImages.clear();
        super.onUnbindModel();
    }

    private void addMargin(SpannableStringBuilder spannable, int marginDp) {
        ColorDrawable spacer = new ColorDrawable(Color.TRANSPARENT);
        int width = (int) LayoutUtils.dpToPx(marginDp, getContext());
        spacer.setBounds(0, 0, width, 1);
        ImageSpan margin = new ImageSpan(spacer);
        spannable.append(" ");
        spannable.setSpan(margin, spannable.length() - 1, spannable.length(),
                Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    /**
     * Sets the width and height of the drawable based on the {@code imageStyle}, preserving aspect
     * ratio. If no dimensions are set in the style, defaults to matching the line height of the
     * {@code textView}.
     */
    @VisibleForTesting
    void setBounds(Drawable imageDrawable, StyleProvider imageStyle, TextView textView) {
        int lineHeight = textView.getLineHeight();

        int styleHeight = styleToImageDim(imageStyle.getHeightSpecPx(getContext()));
        int styleWidth = styleToImageDim(imageStyle.getWidthSpecPx(getContext()));

        int height;
        int width;
        if (styleHeight >= 0 && styleWidth >= 0) {
            // Scale the image to exactly this size.
            height = styleHeight;
            width = styleWidth;
        } else if (styleWidth >= 0) {
            // Set the width, and preserve aspect ratio.
            width = styleWidth;
            height = (int) (imageDrawable.getIntrinsicHeight()
                    * ((float) width / imageDrawable.getIntrinsicWidth()));
        } else {
            // Default to making the image the same height as the text, preserving aspect ratio.
            height = styleHeight >= 0 ? styleHeight : lineHeight;
            width = (int) (imageDrawable.getIntrinsicWidth()
                    * ((float) height / imageDrawable.getIntrinsicHeight()));
        }

        imageDrawable.setBounds(0, 0, width, height);
    }

    private int styleToImageDim(int styleDimensionInPx) {
        return styleDimensionInPx < 0 ? DIMENSION_UNKNOWN : styleDimensionInPx;
    }

    static class ActionsClickableSpan extends ClickableSpan {
        private final Action mAction;
        private final ActionHandler mHandler;
        private final Frame mFrame;

        ActionsClickableSpan(Action action, ActionHandler handler, Frame frame) {
            this.mAction = action;
            this.mHandler = handler;
            this.mFrame = frame;
        }

        @Override
        public void onClick(View widget) {
            // TODO: Pass VE information with the action.
            mHandler.handleAction(
                    mAction, ActionType.CLICK, mFrame, widget, LogData.getDefaultInstance());
        }

        @Override
        public void updateDrawState(TextPaint textPaint) {
            textPaint.setUnderlineText(false);
        }
    }

    /**
     * Triggers click handlers when text is touched; copied from LinkMovementMethod.onTouchEvent.
     *
     * <p>We can't use LinkMovementMethod because that makes the TextView scrollable (relevant when
     * max_lines is set). We could extend TextView and override onScroll to no-op, but ellipsizing
     * will still be broken in that case.
     */
    static class ChunkedTextTouchListener implements View.OnTouchListener {
        private final SpannableStringBuilder mSpannable;

        ChunkedTextTouchListener(SpannableStringBuilder spannable) {
            this.mSpannable = spannable;
        }

        @Override
        public boolean onTouch(View widget, MotionEvent event) {
            int action = event.getAction();

            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_DOWN) {
                TextView textView = (TextView) widget;
                int x = (int) event.getX();
                int y = (int) event.getY();

                x -= textView.getTotalPaddingLeft();
                y -= textView.getTotalPaddingTop();

                x += textView.getScrollX();
                y += textView.getScrollY();

                Layout layout = textView.getLayout();
                int line = layout.getLineForVertical(y);
                int off = layout.getOffsetForHorizontal(line, x);

                ClickableSpan[] links = mSpannable.getSpans(off, off, ClickableSpan.class);

                if (links.length != 0) {
                    if (action == MotionEvent.ACTION_UP) {
                        links[0].onClick(textView);
                    }
                    return true;
                }
            }
            return false;
        }
    }

    @VisibleForTesting
    class ImageSpanDrawableCallback implements Consumer<Drawable> {
        private final LayerDrawable mWrapper;
        private final StyleProvider mImageStyle;
        @Nullable
        private final Integer mOverlayColor;
        private final TextView mTextView;
        private boolean mCancelled;

        ImageSpanDrawableCallback(LayerDrawable wrapper, StyleProvider imageStyle,
                @Nullable Integer overlayColor, TextView textView) {
            this.mWrapper = wrapper;
            this.mImageStyle = imageStyle;
            this.mOverlayColor = overlayColor;
            this.mTextView = textView;
        }

        @Override
        public void accept(@Nullable Drawable imageDrawable) {
            if (mCancelled || imageDrawable == null) {
                return;
            }
            Drawable finalDrawable = ViewUtils.applyOverlayColor(imageDrawable, mOverlayColor);
            checkState(mWrapper.setDrawableByLayerId(SINGLE_LAYER_ID, finalDrawable),
                    "Failed to set drawable on chunked text");
            setBounds(finalDrawable, mImageStyle, mTextView);
            setBounds(mWrapper, mImageStyle, mTextView);
            mTextView.invalidate();
            mLoadingImages.remove(this);
        }

        void cancelCallback() {
            mCancelled = true;
        }
    }

    static class KeySupplier extends TextElementKeySupplier<ChunkedTextElementAdapter> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public ChunkedTextElementAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new ChunkedTextElementAdapter(context, parameters);
        }
    }
}

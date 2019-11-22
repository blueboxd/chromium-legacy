// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.android.libraries.feed.piet.ChunkedTextElementAdapter.SINGLE_LAYER_ID;
import static com.google.common.truth.Truth.assertThat;
import static com.google.search.now.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_BINDING_VALUE;
import static com.google.search.now.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.text.SpannableStringBuilder;
import android.text.SpannedString;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.StyleSpan;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.AdapterFactory.SingletonKeySupplier;
import com.google.android.libraries.feed.piet.ChunkedTextElementAdapter.ActionsClickableSpan;
import com.google.android.libraries.feed.piet.ChunkedTextElementAdapter.ImageSpanDrawableCallback;
import com.google.android.libraries.feed.piet.ChunkedTextElementAdapterTest.ShadowTextViewWithHeight;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.ActionHandler.ActionType;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.ActionsProto.Action;
import com.google.search.now.ui.piet.ActionsProto.Actions;
import com.google.search.now.ui.piet.BindingRefsProto.ActionsBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ImageBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietProto.Frame;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.Font;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.Chunk;
import com.google.search.now.ui.piet.TextProto.ChunkedText;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;
import com.google.search.now.ui.piet.TextProto.StyledImageChunk;
import com.google.search.now.ui.piet.TextProto.StyledTextChunk;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowTextView;

/** Tests of the {@link ChunkedTextElementAdapter}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = ShadowTextViewWithHeight.class)
public class ChunkedTextElementAdapterTest {
    private static final StyleIdsStack CHUNK_STYLE =
            StyleIdsStack.newBuilder().addStyleIds("roasted").build();
    private static final String CHUNKY_TEXT = "Skippy";
    private static final String PROCESSED_TEXT = "smooth";
    private static final ParameterizedText PARAMETERIZED_CHUNK_TEXT =
            ParameterizedText.newBuilder().setText(CHUNKY_TEXT).build();
    private static final Chunk TEXT_CHUNK =
            Chunk.newBuilder()
                    .setTextChunk(StyledTextChunk.newBuilder()
                                          .setParameterizedText(PARAMETERIZED_CHUNK_TEXT)
                                          .setStyleReferences(CHUNK_STYLE))
                    .build();
    private static final ChunkedText CHUNKED_TEXT_TEXT =
            ChunkedText.newBuilder().addChunks(TEXT_CHUNK).build();
    private static final String CHUNKY_URL = "pb.com/jif";
    private static final Image IMAGE_CHUNK_IMAGE =
            Image.newBuilder().addSources(ImageSource.newBuilder().setUrl(CHUNKY_URL)).build();
    private static final Chunk IMAGE_CHUNK =
            Chunk.newBuilder()
                    .setImageChunk(StyledImageChunk.newBuilder()
                                           .setImage(IMAGE_CHUNK_IMAGE)
                                           .setStyleReferences(CHUNK_STYLE))
                    .build();
    private static final String BINDING_ID = "PB";
    private static final ChunkedTextBindingRef CHUNKED_TEXT_BINDING_REF =
            ChunkedTextBindingRef.newBuilder().setBindingId(BINDING_ID).build();

    private static final int STYLE_HEIGHT_PX = 6;
    private static final int STYLE_ASPECT_RATIO = 2;
    private static final int STYLE_WIDTH_PX = STYLE_HEIGHT_PX * STYLE_ASPECT_RATIO;

    private static final int HEIGHT_DP = 6;
    private static final int WIDTH_DP = HEIGHT_DP * STYLE_ASPECT_RATIO;

    private static final int IMAGE_HEIGHT_PX = 60;
    private static final int IMAGE_ASPECT_RATIO = 3;
    private static final int IMAGE_WIDTH_PX = IMAGE_HEIGHT_PX * IMAGE_ASPECT_RATIO;

    private static final int TEXT_HEIGHT = 12;

    @Mock
    private FrameContext frameContext;
    @Mock
    private StyleProvider mockStyleProvider;
    @Mock
    private ParameterizedTextEvaluator mockTextEvaluator;
    @Mock
    private AssetProvider mockAssetProvider;
    @Mock
    private ActionHandler mockActionHandler;
    @Mock
    private HostProviders hostProviders;

    private Drawable drawable;
    private SpannableStringBuilder spannable;

    private Context context;
    private TextView textView;

    private AdapterParameters adapterParameters;

    private ChunkedTextElementAdapter adapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        drawable = new BitmapDrawable(
                Bitmap.createBitmap(IMAGE_WIDTH_PX, IMAGE_HEIGHT_PX, Bitmap.Config.ARGB_8888));
        Shadows.shadowOf(drawable).setIntrinsicHeight(IMAGE_HEIGHT_PX);
        Shadows.shadowOf(drawable).setIntrinsicWidth(IMAGE_WIDTH_PX);
        spannable = new SpannableStringBuilder();

        textView = new TextView(context);

        when(mockTextEvaluator.evaluate(mockAssetProvider,
                     ParameterizedText.newBuilder().setText(CHUNKY_TEXT).build()))
                .thenReturn(PROCESSED_TEXT);
        when(frameContext.makeStyleFor(CHUNK_STYLE)).thenReturn(mockStyleProvider);
        when(frameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(frameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        when(hostProviders.getAssetProvider()).thenReturn(mockAssetProvider);
        when(mockAssetProvider.isRtL()).thenReturn(false);
        when(frameContext.getActionHandler()).thenReturn(mockActionHandler);
        when(mockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        when(mockStyleProvider.getMargins()).thenReturn(EdgeWidths.getDefaultInstance());
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(DIMENSION_UNKNOWN);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(DIMENSION_UNKNOWN);

        adapterParameters = new AdapterParameters(
                null, null, hostProviders, mockTextEvaluator, null, null, new FakeClock());

        when(frameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(adapterParameters.defaultStyleProvider);

        adapter =
                new ChunkedTextElementAdapter.KeySupplier().getAdapter(context, adapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(adapter).isNotNull();
    }

    @Test
    public void testBind_chunkedText() {
        TextElement chunkedTextElement =
                TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build();

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_chunkedTextBinding() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();

        when(frameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build());

        adapter.createAdapter(asElement(chunkedTextBindingElement), frameContext);
        adapter.bindModel(asElement(chunkedTextBindingElement), frameContext);

        verify(frameContext).getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF);
        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_chunkedTextWithBoundText() {
        ParameterizedTextBindingRef textBinding =
                ParameterizedTextBindingRef.newBuilder().setBindingId("sometext").build();
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setTextChunk(
                                        StyledTextChunk.newBuilder().setParameterizedTextBinding(
                                                textBinding))))
                        .build();
        when(frameContext.getParameterizedTextBindingValue(textBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setParameterizedText(PARAMETERIZED_CHUNK_TEXT)
                                    .build());

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_chunkedTextWithBoundImage() {
        ImageBindingRef imageBinding =
                ImageBindingRef.newBuilder().setBindingId("image-binding-id").build();
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setImageChunk(
                                        StyledImageChunk.newBuilder().setImageBinding(
                                                imageBinding))))
                        .build();
        when(frameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.newBuilder().setImage(IMAGE_CHUNK_IMAGE).build());

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(" ");
        ImageSpan[] imageSpans =
                ((SpannedString) adapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(1);

        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);

        verify(mockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), eq(DIMENSION_UNKNOWN), eq(DIMENSION_UNKNOWN),
                        imageCallbackCaptor.capture());

        // Activate the image loading callback
        Drawable imageDrawable = new ColorDrawable(123);
        imageCallbackCaptor.getValue().accept(imageDrawable);

        // Assert that we set the image on the span
        assertThat(containerDrawable.getDrawable(SINGLE_LAYER_ID)).isSameInstanceAs(imageDrawable);
    }

    @Test
    public void testBind_styledTextChunkWithNoContent() {
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setTextChunk(
                                        StyledTextChunk.getDefaultInstance())))
                        .build();
        when(mockTextEvaluator.evaluate(mockAssetProvider, ParameterizedText.getDefaultInstance()))
                .thenReturn("");

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEmpty();
        verify(frameContext)
                .reportMessage(MessageType.ERROR, ERR_MISSING_OR_UNHANDLED_CONTENT,
                        "StyledTextChunk missing ParameterizedText content; has CONTENT_NOT_SET");
    }

    @Test
    public void testBind_chunkedTextWithOptionalBindingNoImage() {
        ImageBindingRef imageBinding = ImageBindingRef.newBuilder()
                                               .setBindingId("image-binding-id")
                                               .setIsOptional(true)
                                               .build();
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setImageChunk(
                                        StyledImageChunk.newBuilder().setImageBinding(
                                                imageBinding))))
                        .build();
        when(frameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEmpty();
        ImageSpan[] imageSpans =
                ((SpannedString) adapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(0);
        verify(frameContext, never()).reportMessage(anyInt(), any(ErrorCode.class), anyString());
    }

    @Test
    public void testBind_chunkedTextWithNoImage() {
        ImageBindingRef imageBinding =
                ImageBindingRef.newBuilder().setBindingId("image-binding-id").build();
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setImageChunk(
                                        StyledImageChunk.newBuilder().setImageBinding(
                                                imageBinding))))
                        .build();
        when(frameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEmpty();
        verify(frameContext)
                .reportMessage(MessageType.ERROR, ERR_MISSING_BINDING_VALUE,
                        "No image found for binding id: image-binding-id");
    }

    @Test
    public void testBind_styledImageChunkWithNoContent() {
        TextElement chunkedTextElement =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.newBuilder().setImageChunk(
                                        StyledImageChunk.getDefaultInstance())))
                        .build();
        when(mockTextEvaluator.evaluate(mockAssetProvider, ParameterizedText.getDefaultInstance()))
                .thenReturn("");

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEmpty();
        ImageSpan[] imageSpans =
                ((SpannedString) adapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(0);

        verify(frameContext)
                .reportMessage(MessageType.ERROR, ERR_MISSING_OR_UNHANDLED_CONTENT,
                        "StyledImageChunk missing Image content; has CONTENT_NOT_SET");
    }

    @Test
    public void testBind_wrongContent_fails() {
        TextElement elementWithWrongContent =
                TextElement.newBuilder()
                        .setParameterizedText(ParameterizedText.getDefaultInstance())
                        .build();

        adapter.createAdapter(asElement(elementWithWrongContent), frameContext);

        assertThatRunnable(
                () -> adapter.bindModel(asElement(elementWithWrongContent), frameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of TextElement");
    }

    @Test
    public void testBind_missingContent_fails() {
        TextElement emptyElement = TextElement.getDefaultInstance();

        assertThatRunnable(() -> adapter.bindModel(asElement(emptyElement), frameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of TextElement");
    }

    @Test
    public void testBind_textChunk() {
        TextElement chunkedTextElement =
                TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build();

        adapter.createAdapter(asElement(chunkedTextElement), frameContext);
        adapter.bindModel(asElement(chunkedTextElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_imageChunk() {
        TextElement chunkedImageElement =
                TextElement.newBuilder()
                        .setChunkedText(ChunkedText.newBuilder().addChunks(IMAGE_CHUNK))
                        .build();

        adapter.createAdapter(asElement(chunkedImageElement), frameContext);
        adapter.bindModel(asElement(chunkedImageElement), frameContext);

        assertThat(adapter.getBaseView().getText().toString()).isEqualTo(" ");
        assertThat(
                ((SpannedString) adapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class))
                .hasLength(1);
        assertThat(((SpannedString) adapter.getBaseView().getText())
                           .getSpans(0, 1, ImageSpan.class)[0]
                           .getVerticalAlignment())
                .isEqualTo(ImageSpan.ALIGN_BASELINE);
    }

    @Test
    public void testBind_emptyChunk_fails() {
        TextElement elementWithEmptyChunk =
                TextElement.newBuilder()
                        .setChunkedText(
                                ChunkedText.newBuilder().addChunks(Chunk.getDefaultInstance()))
                        .build();

        adapter.createAdapter(asElement(elementWithEmptyChunk), frameContext);

        assertThatRunnable(() -> adapter.bindModel(asElement(elementWithEmptyChunk), frameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of ChunkedText Chunk");
    }

    @Test
    public void testSetTextOnView_optionalAbsent() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();
        when(frameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build());
        adapter.createAdapter(asElement(chunkedTextBindingElement), frameContext);

        ChunkedTextBindingRef optionalBindingRef =
                CHUNKED_TEXT_BINDING_REF.toBuilder().setIsOptional(true).build();
        TextElement chunkedTextBindingElementOptional =
                TextElement.newBuilder().setChunkedTextBinding(optionalBindingRef).build();
        when(frameContext.getChunkedTextBindingValue(optionalBindingRef))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.setTextOnView(frameContext, chunkedTextBindingElementOptional);
        assertThat(adapter.getBaseView().getText().toString()).isEmpty();
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testSetTextOnView_noContent() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();
        adapter.createAdapter(
                asElement(TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build()),
                frameContext);

        when(frameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setVisibility(Visibility.INVISIBLE)
                                    .build());

        assertThatRunnable(() -> adapter.setTextOnView(frameContext, chunkedTextBindingElement))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Chunked text binding PB had no content");
    }

    @Test
    public void testAddTextChunk_setsTextAfterEvaluatingParameterizedText() {
        adapter.addTextChunk(frameContext, spannable, TEXT_CHUNK);

        verify(mockTextEvaluator)
                .evaluate(mockAssetProvider, TEXT_CHUNK.getTextChunk().getParameterizedText());

        assertThat(spannable.toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testAddTextChunk_setsStyles() {
        int color = 314159;
        context.getResources().getDisplayMetrics().scaledDensity = 1.5f;
        Font font = Font.newBuilder().setItalic(true).setSize(20).build();
        // The text scales with accessibility settings: size (20) x scaledDensity (1.5) = final size
        // 30
        int expectedTextSize = 30;

        when(mockStyleProvider.hasColor()).thenReturn(true);
        when(mockStyleProvider.getColor()).thenReturn(color);
        when(mockStyleProvider.getFont()).thenReturn(font);

        adapter.addTextChunk(frameContext, spannable, TEXT_CHUNK);

        assertThat(spannable.getSpans(0, PROCESSED_TEXT.length(), Object.class)).hasLength(3);

        ForegroundColorSpan[] colorSpans =
                spannable.getSpans(0, PROCESSED_TEXT.length(), ForegroundColorSpan.class);
        assertThat(colorSpans[0].getForegroundColor()).isEqualTo(color);

        AbsoluteSizeSpan[] sizeSpans =
                spannable.getSpans(0, PROCESSED_TEXT.length(), AbsoluteSizeSpan.class);
        assertThat(sizeSpans[0].getSize()).isEqualTo(expectedTextSize);

        StyleSpan[] styleSpans = spannable.getSpans(0, PROCESSED_TEXT.length(), StyleSpan.class);
        assertThat(styleSpans[0].getStyle()).isEqualTo(Typeface.ITALIC);
    }

    @Test
    public void testAddTextChunk_addsMargins() {
        when(mockStyleProvider.hasWidth()).thenReturn(true);
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(STYLE_WIDTH_PX);
        when(mockStyleProvider.hasHeight()).thenReturn(true);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(STYLE_HEIGHT_PX);

        when(mockStyleProvider.getMargins())
                .thenReturn(EdgeWidths.newBuilder().setStart(11).setEnd(22).build());

        // Required to set up the local frameContext member var.
        adapter.createAdapter(asElement(TextElement.getDefaultInstance()), frameContext);

        adapter.addTextChunk(frameContext, spannable, TEXT_CHUNK);

        assertThat(spannable.toString()).isEqualTo(" smooth ");

        ImageSpan[] imageSpans = spannable.getSpans(0, 8, ImageSpan.class);

        Drawable leftMarginDrawable = imageSpans[0].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(leftMarginDrawable.getBounds().right).isEqualTo(11);

        Drawable rightMarginDrawable = imageSpans[1].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(rightMarginDrawable.getBounds().right).isEqualTo(22);
    }

    @Test
    public void testAddImageChunk_setsImageAndDims() {
        when(mockStyleProvider.hasWidth()).thenReturn(true);
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(STYLE_WIDTH_PX);
        when(mockStyleProvider.hasHeight()).thenReturn(true);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(STYLE_HEIGHT_PX);
        when(mockStyleProvider.hasPreLoadFill()).thenReturn(true);
        Drawable preLoadFill = new ColorDrawable(Color.CYAN);
        when(mockStyleProvider.createPreLoadFill()).thenReturn(preLoadFill);

        // Required to set up the local frameContext member var.
        adapter.createAdapter(asElement(TextElement.getDefaultInstance()), frameContext);

        adapter.addImageChunk(frameContext, textView, spannable, IMAGE_CHUNK);

        assertThat(spannable.toString()).isEqualTo(" ");

        ImageSpan[] imageSpans = spannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), eq(STYLE_WIDTH_PX), eq(STYLE_HEIGHT_PX),
                        imageCallbackCaptor.capture());

        // Check for the pre-load fill
        assertThat(containerDrawable.getDrawable(0)).isSameInstanceAs(preLoadFill);

        // Activate the image loading callback
        Drawable imageDrawable = new ColorDrawable(123);
        imageCallbackCaptor.getValue().accept(imageDrawable);

        // Assert that we set the image on the span
        assertThat(containerDrawable.getDrawable(0)).isSameInstanceAs(imageDrawable);

        assertThat(imageDrawable.getBounds())
                .isEqualTo(new Rect(0, 0, STYLE_WIDTH_PX, STYLE_HEIGHT_PX));
    }

    @Test
    public void testAddImageChunk_addsMargins() {
        when(mockStyleProvider.hasWidth()).thenReturn(true);
        when(mockStyleProvider.getWidthSpecPx(context)).thenReturn(STYLE_WIDTH_PX);
        when(mockStyleProvider.hasHeight()).thenReturn(true);
        when(mockStyleProvider.getHeightSpecPx(context)).thenReturn(STYLE_HEIGHT_PX);

        when(mockStyleProvider.getMargins())
                .thenReturn(EdgeWidths.newBuilder().setStart(11).setEnd(22).build());

        // Required to set up the local frameContext member var.
        adapter.createAdapter(asElement(TextElement.getDefaultInstance()), frameContext);

        adapter.addImageChunk(frameContext, textView, spannable, IMAGE_CHUNK);

        assertThat(spannable.toString()).isEqualTo("   ");

        ImageSpan[] imageSpans = spannable.getSpans(0, 3, ImageSpan.class);

        Drawable leftMarginDrawable = imageSpans[0].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(leftMarginDrawable.getBounds().right).isEqualTo(11);

        Drawable rightMarginDrawable = imageSpans[2].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(rightMarginDrawable.getBounds().right).isEqualTo(22);
    }

    @Test
    public void testAddImageChunk_tintColor() {
        // Required to set up the local frameContext member var.
        adapter.createAdapter(asElement(TextElement.getDefaultInstance()), frameContext);

        StyleProvider tintStyleProvider = new StyleProvider(
                Style.newBuilder().setStyleId("tint").setColor(0xFEEDFACE).build(),
                mockAssetProvider);
        StyleIdsStack tintStyle = StyleIdsStack.newBuilder().addStyleIds("tint").build();
        when(frameContext.makeStyleFor(tintStyle)).thenReturn(tintStyleProvider);

        Chunk imageChunk = Chunk.newBuilder()
                                   .setImageChunk(StyledImageChunk.newBuilder()
                                                          .setImage(IMAGE_CHUNK_IMAGE)
                                                          .setStyleReferences(tintStyle))
                                   .build();

        adapter.addImageChunk(frameContext, textView, spannable, imageChunk);

        ImageSpan[] imageSpans = spannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), anyInt(), anyInt(), imageCallbackCaptor.capture());

        // Activate the image loading callback
        imageCallbackCaptor.getValue().accept(drawable);

        // Assert that we set the image on the span
        assertThat(containerDrawable.getDrawable(0).getColorFilter())
                .isEqualTo(new PorterDuffColorFilter(0xFEEDFACE, Mode.SRC_IN));
    }

    @Test
    public void testBindSetsActions_inline() {
        TextElement imageChunkWithActions =
                TextElement.newBuilder()
                        .setChunkedText(ChunkedText.newBuilder().addChunks(
                                IMAGE_CHUNK.toBuilder().setActions(
                                        Actions.newBuilder().setOnClickAction(
                                                Action.getDefaultInstance()))))
                        .build();
        when(frameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        adapter.createAdapter(asElement(imageChunkWithActions), frameContext);
        adapter.bindModel(asElement(imageChunkWithActions), frameContext);

        assertThat(((SpannedString) adapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .hasLength(1);
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        adapter.getBaseView().dispatchTouchEvent(motionEvent);
        verify(mockActionHandler)
                .handleAction(Action.getDefaultInstance(), ActionType.CLICK,
                        Frame.getDefaultInstance(), adapter.getBaseView(),
                        LogData.getDefaultInstance());
    }

    @Test
    public void testBindSetsActions_bind() {
        String bindingId = "ACTION";
        ActionsBindingRef binding = ActionsBindingRef.newBuilder().setBindingId(bindingId).build();
        TextElement imageChunkWithActions =
                TextElement.newBuilder()
                        .setChunkedText(ChunkedText.newBuilder().addChunks(
                                IMAGE_CHUNK.toBuilder().setActionsBinding(binding)))
                        .build();
        when(frameContext.getActionsFromBinding(binding))
                .thenReturn(
                        Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build());
        when(frameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        adapter.createAdapter(asElement(imageChunkWithActions), frameContext);
        adapter.bindModel(asElement(imageChunkWithActions), frameContext);

        assertThat(((SpannedString) adapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .hasLength(1);
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        adapter.getBaseView().dispatchTouchEvent(motionEvent);
        verify(mockActionHandler)
                .handleAction(Action.getDefaultInstance(), ActionType.CLICK,
                        Frame.getDefaultInstance(), adapter.getBaseView(),
                        LogData.getDefaultInstance());
    }

    @Test
    public void testBindSetsActions_bindingNotFound() {
        String bindingId = "ACTION";
        ActionsBindingRef binding = ActionsBindingRef.newBuilder().setBindingId(bindingId).build();
        TextElement imageChunkWithActions =
                TextElement.newBuilder()
                        .setChunkedText(ChunkedText.newBuilder().addChunks(
                                IMAGE_CHUNK.toBuilder().setActionsBinding(binding)))
                        .build();
        when(frameContext.getActionsFromBinding(binding)).thenReturn(Actions.getDefaultInstance());
        when(frameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        adapter.createAdapter(asElement(imageChunkWithActions), frameContext);
        adapter.bindModel(asElement(imageChunkWithActions), frameContext);

        // Completes successfully, but doesn't add the clickable span.
        assertThat(((SpannedString) adapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .isEmpty();
    }

    @Test
    public void testUnbind_cancelsCallbacks() {
        adapter.createAdapter(asElement(TextElement.getDefaultInstance()), frameContext);
        adapter.addImageChunk(frameContext, textView, spannable, IMAGE_CHUNK);

        ImageSpan[] imageSpans = spannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();
        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), eq(DIMENSION_UNKNOWN), eq(DIMENSION_UNKNOWN),
                        imageCallbackCaptor.capture());

        // Unbind the model
        adapter.unbindModel();

        // Activate the image loading callback
        Drawable imageDrawable = new ColorDrawable(Color.RED);
        imageCallbackCaptor.getValue().accept(imageDrawable);

        // Assert that we did NOT set the image on the span
        assertThat(containerDrawable.getDrawable(SINGLE_LAYER_ID))
                .isNotSameInstanceAs(imageDrawable);
    }

    @Test
    public void testKeySupplier() {
        ChunkedTextElementAdapter.KeySupplier keySupplier =
                new ChunkedTextElementAdapter.KeySupplier();
        assertThat(keySupplier.getAdapterTag()).isEqualTo("ChunkedTextElementAdapter");
        assertThat(keySupplier.getAdapter(context, adapterParameters)).isNotNull();
        assertThat(keySupplier).isNotInstanceOf(SingletonKeySupplier.class);
    }

    @Test
    public void testSetBounds_heightAndWidth_setsBoth() {
        int widthDp = 123;
        int heightDp = 456;
        adapter.setBounds(drawable,
                new StyleProvider(Style.newBuilder().setHeight(heightDp).setWidth(widthDp).build(),
                        mockAssetProvider),
                textView);

        int widthPx = (int) LayoutUtils.dpToPx(widthDp, context);
        int heightPx = (int) LayoutUtils.dpToPx(heightDp, context);
        assertThat(drawable.getBounds()).isEqualTo(new Rect(0, 0, widthPx, heightPx));
    }

    @Test
    public void testSetBounds_heightOnly_aspectRatioScaled() {
        adapter.setBounds(drawable,
                new StyleProvider(
                        Style.newBuilder().setHeight(HEIGHT_DP).build(), mockAssetProvider),
                textView);

        int heightPx = (int) LayoutUtils.dpToPx(HEIGHT_DP, context);
        assertThat(drawable.getBounds())
                .isEqualTo(new Rect(0, 0, heightPx * IMAGE_ASPECT_RATIO, heightPx));
    }

    @Test
    public void testSetBounds_widthOnly_aspectRatioScaled() {
        adapter.setBounds(drawable,
                new StyleProvider(Style.newBuilder().setWidth(WIDTH_DP).build(), mockAssetProvider),
                textView);

        int widthPx = (int) LayoutUtils.dpToPx(WIDTH_DP, context);
        assertThat(drawable.getBounds())
                .isEqualTo(new Rect(0, 0, widthPx, widthPx / IMAGE_ASPECT_RATIO));
    }

    @Test
    public void testSetBounds_noHeightOrWidth_defaultsToTextHeight() {
        adapter.setBounds(drawable,
                new StyleProvider(Style.getDefaultInstance(), mockAssetProvider), textView);

        assertThat(drawable.getBounds())
                .isEqualTo(new Rect(0, 0, TEXT_HEIGHT * IMAGE_ASPECT_RATIO, TEXT_HEIGHT));
    }

    @Implements(TextView.class)
    public static class ShadowTextViewWithHeight extends ShadowTextView {
        @Implementation
        public int getLineHeight() {
            return TEXT_HEIGHT;
        }
    }

    private static Element asElement(TextElement textElement) {
        return Element.newBuilder().setTextElement(textElement).build();
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.library.piet.ChunkedTextElementAdapter.SINGLE_LAYER_ID;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_BINDING_VALUE;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowTextView;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.SingletonKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.ChunkedTextElementAdapter.ActionsClickableSpan;
import org.chromium.chrome.browser.feed.library.piet.ChunkedTextElementAdapter.ImageSpanDrawableCallback;
import org.chromium.chrome.browser.feed.library.piet.ChunkedTextElementAdapterTest.ShadowTextViewWithHeight;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ActionsBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ChunkedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ImageBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.EdgeWidths;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Font;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.Chunk;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ChunkedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.StyledImageChunk;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.StyledTextChunk;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ChunkedTextElementAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowTextViewWithHeight.class)
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
    private FrameContext mFrameContext;
    @Mock
    private StyleProvider mMockStyleProvider;
    @Mock
    private ParameterizedTextEvaluator mMockTextEvaluator;
    @Mock
    private AssetProvider mMockAssetProvider;
    @Mock
    private ActionHandler mMockActionHandler;
    @Mock
    private HostProviders mHostProviders;

    private Drawable mDrawable;
    private SpannableStringBuilder mSpannable;

    private Context mContext;
    private TextView mTextView;

    private AdapterParameters mAdapterParameters;

    private ChunkedTextElementAdapter mAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        mDrawable = new BitmapDrawable(
                Bitmap.createBitmap(IMAGE_WIDTH_PX, IMAGE_HEIGHT_PX, Bitmap.Config.ARGB_8888));
        Shadows.shadowOf(mDrawable).setIntrinsicHeight(IMAGE_HEIGHT_PX);
        Shadows.shadowOf(mDrawable).setIntrinsicWidth(IMAGE_WIDTH_PX);
        mSpannable = new SpannableStringBuilder();

        mTextView = new TextView(mContext);

        when(mMockTextEvaluator.evaluate(mMockAssetProvider,
                     ParameterizedText.newBuilder().setText(CHUNKY_TEXT).build()))
                .thenReturn(PROCESSED_TEXT);
        when(mFrameContext.makeStyleFor(CHUNK_STYLE)).thenReturn(mMockStyleProvider);
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(mFrameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);
        when(mHostProviders.getAssetProvider()).thenReturn(mMockAssetProvider);
        when(mMockAssetProvider.isRtL()).thenReturn(false);
        when(mFrameContext.getActionHandler()).thenReturn(mMockActionHandler);
        when(mMockStyleProvider.getFont()).thenReturn(Font.getDefaultInstance());
        when(mMockStyleProvider.getMargins()).thenReturn(EdgeWidths.getDefaultInstance());
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(DIMENSION_UNKNOWN);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(DIMENSION_UNKNOWN);

        mAdapterParameters = new AdapterParameters(
                null, null, mHostProviders, mMockTextEvaluator, null, null, new FakeClock());

        when(mFrameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(mAdapterParameters.mDefaultStyleProvider);

        mAdapter = new ChunkedTextElementAdapter.KeySupplier().getAdapter(
                mContext, mAdapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(mAdapter).isNotNull();
    }

    @Test
    public void testBind_chunkedText() {
        TextElement chunkedTextElement =
                TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build();

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_chunkedTextBinding() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();

        when(mFrameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build());

        mAdapter.createAdapter(asElement(chunkedTextBindingElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextBindingElement), mFrameContext);

        verify(mFrameContext).getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF);
        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
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
        when(mFrameContext.getParameterizedTextBindingValue(textBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setParameterizedText(PARAMETERIZED_CHUNK_TEXT)
                                    .build());

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
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
        when(mFrameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.newBuilder().setImage(IMAGE_CHUNK_IMAGE).build());

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(" ");
        ImageSpan[] imageSpans =
                ((SpannedString) mAdapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(1);

        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);

        verify(mMockAssetProvider)
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
        when(mMockTextEvaluator.evaluate(
                     mMockAssetProvider, ParameterizedText.getDefaultInstance()))
                .thenReturn("");

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEmpty();
        verify(mFrameContext)
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
        when(mFrameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEmpty();
        ImageSpan[] imageSpans =
                ((SpannedString) mAdapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(0);
        verify(mFrameContext, never()).reportMessage(anyInt(), any(ErrorCode.class), anyString());
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
        when(mFrameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEmpty();
        verify(mFrameContext)
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
        when(mMockTextEvaluator.evaluate(
                     mMockAssetProvider, ParameterizedText.getDefaultInstance()))
                .thenReturn("");

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEmpty();
        ImageSpan[] imageSpans =
                ((SpannedString) mAdapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class);
        assertThat(imageSpans).hasLength(0);

        verify(mFrameContext)
                .reportMessage(MessageType.ERROR, ERR_MISSING_OR_UNHANDLED_CONTENT,
                        "StyledImageChunk missing Image content; has CONTENT_NOT_SET");
    }

    @Test
    public void testBind_wrongContent_fails() {
        TextElement elementWithWrongContent =
                TextElement.newBuilder()
                        .setParameterizedText(ParameterizedText.getDefaultInstance())
                        .build();

        mAdapter.createAdapter(asElement(elementWithWrongContent), mFrameContext);

        assertThatRunnable(
                () -> mAdapter.bindModel(asElement(elementWithWrongContent), mFrameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of TextElement");
    }

    @Test
    public void testBind_missingContent_fails() {
        TextElement emptyElement = TextElement.getDefaultInstance();

        assertThatRunnable(() -> mAdapter.bindModel(asElement(emptyElement), mFrameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of TextElement");
    }

    @Test
    public void testBind_textChunk() {
        TextElement chunkedTextElement =
                TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build();

        mAdapter.createAdapter(asElement(chunkedTextElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedTextElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testBind_imageChunk() {
        TextElement chunkedImageElement =
                TextElement.newBuilder()
                        .setChunkedText(ChunkedText.newBuilder().addChunks(IMAGE_CHUNK))
                        .build();

        mAdapter.createAdapter(asElement(chunkedImageElement), mFrameContext);
        mAdapter.bindModel(asElement(chunkedImageElement), mFrameContext);

        assertThat(mAdapter.getBaseView().getText().toString()).isEqualTo(" ");
        assertThat(
                ((SpannedString) mAdapter.getBaseView().getText()).getSpans(0, 1, ImageSpan.class))
                .hasLength(1);
        assertThat(((SpannedString) mAdapter.getBaseView().getText())
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

        mAdapter.createAdapter(asElement(elementWithEmptyChunk), mFrameContext);

        assertThatRunnable(
                () -> mAdapter.bindModel(asElement(elementWithEmptyChunk), mFrameContext))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Unhandled type of ChunkedText Chunk");
    }

    @Test
    public void testSetTextOnView_optionalAbsent() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();
        when(mFrameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build());
        mAdapter.createAdapter(asElement(chunkedTextBindingElement), mFrameContext);

        ChunkedTextBindingRef optionalBindingRef =
                CHUNKED_TEXT_BINDING_REF.toBuilder().setIsOptional(true).build();
        TextElement chunkedTextBindingElementOptional =
                TextElement.newBuilder().setChunkedTextBinding(optionalBindingRef).build();
        when(mFrameContext.getChunkedTextBindingValue(optionalBindingRef))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.setTextOnView(mFrameContext, chunkedTextBindingElementOptional);
        assertThat(mAdapter.getBaseView().getText().toString()).isEmpty();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testSetTextOnView_noContent() {
        TextElement chunkedTextBindingElement =
                TextElement.newBuilder().setChunkedTextBinding(CHUNKED_TEXT_BINDING_REF).build();
        mAdapter.createAdapter(
                asElement(TextElement.newBuilder().setChunkedText(CHUNKED_TEXT_TEXT).build()),
                mFrameContext);

        when(mFrameContext.getChunkedTextBindingValue(CHUNKED_TEXT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(BINDING_ID)
                                    .setVisibility(Visibility.INVISIBLE)
                                    .build());

        assertThatRunnable(() -> mAdapter.setTextOnView(mFrameContext, chunkedTextBindingElement))
                .throwsAnExceptionOfType(IllegalArgumentException.class)
                .that()
                .hasMessageThat()
                .contains("Chunked text binding PB had no content");
    }

    @Test
    public void testAddTextChunk_setsTextAfterEvaluatingParameterizedText() {
        mAdapter.addTextChunk(mFrameContext, mSpannable, TEXT_CHUNK);

        verify(mMockTextEvaluator)
                .evaluate(mMockAssetProvider, TEXT_CHUNK.getTextChunk().getParameterizedText());

        assertThat(mSpannable.toString()).isEqualTo(PROCESSED_TEXT);
    }

    @Test
    public void testAddTextChunk_setsStyles() {
        int color = 314159;
        mContext.getResources().getDisplayMetrics().scaledDensity = 1.5f;
        Font font = Font.newBuilder().setItalic(true).setSize(20).build();
        // The text scales with accessibility settings: size (20) x scaledDensity (1.5) = final size
        // 30
        int expectedTextSize = 30;

        when(mMockStyleProvider.hasColor()).thenReturn(true);
        when(mMockStyleProvider.getColor()).thenReturn(color);
        when(mMockStyleProvider.getFont()).thenReturn(font);

        mAdapter.addTextChunk(mFrameContext, mSpannable, TEXT_CHUNK);

        assertThat(mSpannable.getSpans(0, PROCESSED_TEXT.length(), Object.class)).hasLength(3);

        ForegroundColorSpan[] colorSpans =
                mSpannable.getSpans(0, PROCESSED_TEXT.length(), ForegroundColorSpan.class);
        assertThat(colorSpans[0].getForegroundColor()).isEqualTo(color);

        AbsoluteSizeSpan[] sizeSpans =
                mSpannable.getSpans(0, PROCESSED_TEXT.length(), AbsoluteSizeSpan.class);
        assertThat(sizeSpans[0].getSize()).isEqualTo(expectedTextSize);

        StyleSpan[] styleSpans = mSpannable.getSpans(0, PROCESSED_TEXT.length(), StyleSpan.class);
        assertThat(styleSpans[0].getStyle()).isEqualTo(Typeface.ITALIC);
    }

    @Test
    public void testAddTextChunk_addsMargins() {
        when(mMockStyleProvider.hasWidth()).thenReturn(true);
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(STYLE_WIDTH_PX);
        when(mMockStyleProvider.hasHeight()).thenReturn(true);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(STYLE_HEIGHT_PX);

        when(mMockStyleProvider.getMargins())
                .thenReturn(EdgeWidths.newBuilder().setStart(11).setEnd(22).build());

        // Required to set up the local frameContext member var.
        mAdapter.createAdapter(asElement(TextElement.getDefaultInstance()), mFrameContext);

        mAdapter.addTextChunk(mFrameContext, mSpannable, TEXT_CHUNK);

        assertThat(mSpannable.toString()).isEqualTo(" smooth ");

        ImageSpan[] imageSpans = mSpannable.getSpans(0, 8, ImageSpan.class);

        Drawable leftMarginDrawable = imageSpans[0].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(leftMarginDrawable.getBounds().right).isEqualTo(11);

        Drawable rightMarginDrawable = imageSpans[1].getDrawable();
        assertThat(leftMarginDrawable.getBounds().left).isEqualTo(0);
        assertThat(rightMarginDrawable.getBounds().right).isEqualTo(22);
    }

    @Test
    public void testAddImageChunk_setsImageAndDims() {
        when(mMockStyleProvider.hasWidth()).thenReturn(true);
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(STYLE_WIDTH_PX);
        when(mMockStyleProvider.hasHeight()).thenReturn(true);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(STYLE_HEIGHT_PX);
        when(mMockStyleProvider.hasPreLoadFill()).thenReturn(true);
        Drawable preLoadFill = new ColorDrawable(Color.CYAN);
        when(mMockStyleProvider.createPreLoadFill()).thenReturn(preLoadFill);

        // Required to set up the local frameContext member var.
        mAdapter.createAdapter(asElement(TextElement.getDefaultInstance()), mFrameContext);

        mAdapter.addImageChunk(mFrameContext, mTextView, mSpannable, IMAGE_CHUNK);

        assertThat(mSpannable.toString()).isEqualTo(" ");

        ImageSpan[] imageSpans = mSpannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mMockAssetProvider)
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
        when(mMockStyleProvider.hasWidth()).thenReturn(true);
        when(mMockStyleProvider.getWidthSpecPx(mContext)).thenReturn(STYLE_WIDTH_PX);
        when(mMockStyleProvider.hasHeight()).thenReturn(true);
        when(mMockStyleProvider.getHeightSpecPx(mContext)).thenReturn(STYLE_HEIGHT_PX);

        when(mMockStyleProvider.getMargins())
                .thenReturn(EdgeWidths.newBuilder().setStart(11).setEnd(22).build());

        // Required to set up the local frameContext member var.
        mAdapter.createAdapter(asElement(TextElement.getDefaultInstance()), mFrameContext);

        mAdapter.addImageChunk(mFrameContext, mTextView, mSpannable, IMAGE_CHUNK);

        assertThat(mSpannable.toString()).isEqualTo("   ");

        ImageSpan[] imageSpans = mSpannable.getSpans(0, 3, ImageSpan.class);

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
        mAdapter.createAdapter(asElement(TextElement.getDefaultInstance()), mFrameContext);

        StyleProvider tintStyleProvider = new StyleProvider(
                Style.newBuilder().setStyleId("tint").setColor(0xFEEDFACE).build(),
                mMockAssetProvider);
        StyleIdsStack tintStyle = StyleIdsStack.newBuilder().addStyleIds("tint").build();
        when(mFrameContext.makeStyleFor(tintStyle)).thenReturn(tintStyleProvider);

        Chunk imageChunk = Chunk.newBuilder()
                                   .setImageChunk(StyledImageChunk.newBuilder()
                                                          .setImage(IMAGE_CHUNK_IMAGE)
                                                          .setStyleReferences(tintStyle))
                                   .build();

        mAdapter.addImageChunk(mFrameContext, mTextView, mSpannable, imageChunk);

        ImageSpan[] imageSpans = mSpannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();

        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mMockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), anyInt(), anyInt(), imageCallbackCaptor.capture());

        // Activate the image loading callback
        imageCallbackCaptor.getValue().accept(mDrawable);

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
        when(mFrameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        mAdapter.createAdapter(asElement(imageChunkWithActions), mFrameContext);
        mAdapter.bindModel(asElement(imageChunkWithActions), mFrameContext);

        assertThat(((SpannedString) mAdapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .hasLength(1);
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mAdapter.getBaseView().dispatchTouchEvent(motionEvent);
        verify(mMockActionHandler)
                .handleAction(Action.getDefaultInstance(), ActionType.CLICK,
                        Frame.getDefaultInstance(), mAdapter.getBaseView(),
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
        when(mFrameContext.getActionsFromBinding(binding))
                .thenReturn(
                        Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build());
        when(mFrameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        mAdapter.createAdapter(asElement(imageChunkWithActions), mFrameContext);
        mAdapter.bindModel(asElement(imageChunkWithActions), mFrameContext);

        assertThat(((SpannedString) mAdapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .hasLength(1);
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mAdapter.getBaseView().dispatchTouchEvent(motionEvent);
        verify(mMockActionHandler)
                .handleAction(Action.getDefaultInstance(), ActionType.CLICK,
                        Frame.getDefaultInstance(), mAdapter.getBaseView(),
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
        when(mFrameContext.getActionsFromBinding(binding)).thenReturn(Actions.getDefaultInstance());
        when(mFrameContext.getFrame()).thenReturn(Frame.getDefaultInstance());

        mAdapter.createAdapter(asElement(imageChunkWithActions), mFrameContext);
        mAdapter.bindModel(asElement(imageChunkWithActions), mFrameContext);

        // Completes successfully, but doesn't add the clickable span.
        assertThat(((SpannedString) mAdapter.getBaseView().getText())
                           .getSpans(0, 1, ActionsClickableSpan.class))
                .isEmpty();
    }

    @Test
    public void testUnbind_cancelsCallbacks() {
        mAdapter.createAdapter(asElement(TextElement.getDefaultInstance()), mFrameContext);
        mAdapter.addImageChunk(mFrameContext, mTextView, mSpannable, IMAGE_CHUNK);

        ImageSpan[] imageSpans = mSpannable.getSpans(0, 1, ImageSpan.class);
        LayerDrawable containerDrawable = (LayerDrawable) imageSpans[0].getDrawable();
        ArgumentCaptor<ImageSpanDrawableCallback> imageCallbackCaptor =
                ArgumentCaptor.forClass(ImageSpanDrawableCallback.class);
        verify(mMockAssetProvider)
                .getImage(eq(IMAGE_CHUNK_IMAGE), eq(DIMENSION_UNKNOWN), eq(DIMENSION_UNKNOWN),
                        imageCallbackCaptor.capture());

        // Unbind the model
        mAdapter.unbindModel();

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
        assertThat(keySupplier.getAdapter(mContext, mAdapterParameters)).isNotNull();
        assertThat(keySupplier).isNotInstanceOf(SingletonKeySupplier.class);
    }

    @Test
    public void testSetBounds_heightAndWidth_setsBoth() {
        int widthDp = 123;
        int heightDp = 456;
        mAdapter.setBounds(mDrawable,
                new StyleProvider(Style.newBuilder().setHeight(heightDp).setWidth(widthDp).build(),
                        mMockAssetProvider),
                mTextView);

        int widthPx = (int) LayoutUtils.dpToPx(widthDp, mContext);
        int heightPx = (int) LayoutUtils.dpToPx(heightDp, mContext);
        assertThat(mDrawable.getBounds()).isEqualTo(new Rect(0, 0, widthPx, heightPx));
    }

    @Test
    public void testSetBounds_heightOnly_aspectRatioScaled() {
        mAdapter.setBounds(mDrawable,
                new StyleProvider(
                        Style.newBuilder().setHeight(HEIGHT_DP).build(), mMockAssetProvider),
                mTextView);

        int heightPx = (int) LayoutUtils.dpToPx(HEIGHT_DP, mContext);
        assertThat(mDrawable.getBounds())
                .isEqualTo(new Rect(0, 0, heightPx * IMAGE_ASPECT_RATIO, heightPx));
    }

    @Test
    public void testSetBounds_widthOnly_aspectRatioScaled() {
        mAdapter.setBounds(mDrawable,
                new StyleProvider(
                        Style.newBuilder().setWidth(WIDTH_DP).build(), mMockAssetProvider),
                mTextView);

        int widthPx = (int) LayoutUtils.dpToPx(WIDTH_DP, mContext);
        assertThat(mDrawable.getBounds())
                .isEqualTo(new Rect(0, 0, widthPx, widthPx / IMAGE_ASPECT_RATIO));
    }

    @Test
    public void testSetBounds_noHeightOrWidth_defaultsToTextHeight() {
        mAdapter.setBounds(mDrawable,
                new StyleProvider(Style.getDefaultInstance(), mMockAssetProvider), mTextView);

        assertThat(mDrawable.getBounds())
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

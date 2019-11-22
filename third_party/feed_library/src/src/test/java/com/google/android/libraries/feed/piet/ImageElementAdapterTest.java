// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.PietStylesHelper.PietStylesHelperFactory;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.android.libraries.feed.piet.ui.RoundedCornerWrapperView;
import com.google.search.now.ui.piet.BindingRefsProto.ImageBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.StyleBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition;
import com.google.search.now.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import com.google.search.now.ui.piet.MediaQueriesProto.MediaQueryCondition;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners.Corners;
import com.google.search.now.ui.piet.StylesProto.EdgeWidths;
import com.google.search.now.ui.piet.StylesProto.Style;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link ImageElementAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class ImageElementAdapterTest {
    private static final int HEIGHT_DP = 123;
    private static final int WIDTH_DP = 321;
    private static final EdgeWidths PADDING =
            EdgeWidths.newBuilder().setBottom(1).setTop(2).setStart(3).setEnd(4).build();
    private static final RoundedCorners CORNERS = RoundedCorners.newBuilder()
                                                          .setBitmask(Corners.BOTTOM_START_VALUE)
                                                          .setRadiusDp(34)
                                                          .build();
    private static final Image DEFAULT_IMAGE =
            Image.newBuilder().addSources(ImageSource.newBuilder().setUrl("icanhas.chz")).build();
    private static final Element DEFAULT_MODEL =
            asElement(ImageElement.newBuilder().setImage(DEFAULT_IMAGE).build());
    private static final boolean LEGACY_CORNERS_FLAG;
    private static final boolean OUTLINE_CORNERS_FLAG;

    @Mock
    private ElementAdapterFactory adapterFactory;
    @Mock
    private TemplateBinder templateBinder;
    @Mock
    private FrameContext frameContext;
    @Mock
    private AssetProvider assetProvider;
    @Mock
    private StyleProvider styleProvider;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private LoadImageCallback loadImageCallback;

    private Context context;
    private int heightPx;
    private int widthPx;
    private ImageView imageView;
    private final FakeClock clock = new FakeClock();
    private RoundedCornerMaskCache maskCache;

    private ImageElementAdapterForTest adapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        heightPx = (int) LayoutUtils.dpToPx(HEIGHT_DP, context);
        widthPx = (int) LayoutUtils.dpToPx(WIDTH_DP, context);
        maskCache = new RoundedCornerMaskCache();
        AdapterParameters parameters = new AdapterParameters(context, null, hostProviders, null,
                adapterFactory, templateBinder, clock, new PietStylesHelperFactory(), maskCache,
                LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG);

        when(frameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(styleProvider);
        when(frameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(styleProvider.getPadding()).thenReturn(PADDING);
        when(styleProvider.hasRoundedCorners()).thenReturn(true);
        when(styleProvider.getRoundedCorners()).thenReturn(CORNERS);
        when(styleProvider.getScaleType()).thenReturn(ScaleType.FIT_CENTER);
        when(styleProvider.createWrapperView(
                     context, maskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(new RoundedCornerWrapperView(context, CORNERS, maskCache,
                        Suppliers.of(false),
                        /*radiusOverride= */ 0,
                        /* borders= */ null,
                        /* allowClipPath= */ false,
                        /* allowOutlineRounding= */ false));
        setStyle(null, null);

        adapter = new ImageElementAdapterForTest(context, parameters);
    }

    @Test
    public void testCreate() {
        assertThat(adapter).isNotNull();
    }

    @Test
    public void testCreateAdapter() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        assertThat(adapter.getModel()).isSameInstanceAs(DEFAULT_MODEL.getImageElement());

        assertThat(adapter.getView()).isNotNull();

        assertThat(adapter.getComputedHeightPx()).isEqualTo(heightPx);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(widthPx);
        assertThat(adapter.getBaseView().getCropToPadding()).isTrue();
        verify(styleProvider).applyElementStyles(adapter);
    }

    @Test
    public void testCreateAdapter_noDimensionsSet() {
        setStyle(null, null);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getModel()).isSameInstanceAs(DEFAULT_MODEL.getImageElement());

        assertThat(adapter.getView()).isNotNull();

        // Assert that width and height are set to the defaults
        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testCreateAdapter_heightOnly() {
        setStyle(HEIGHT_DP, null);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getModel()).isEqualTo(DEFAULT_MODEL.getImageElement());

        assertThat(adapter.getView()).isNotNull();

        // Width defaults to MATCH_PARENT
        assertThat(adapter.getComputedHeightPx()).isEqualTo(heightPx);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testCreateAdapter_widthOnly() {
        setStyle(null, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getModel()).isEqualTo(DEFAULT_MODEL.getImageElement());

        assertThat(adapter.getView()).isNotNull();

        // Image defaults to a square.
        assertThat(adapter.getComputedHeightPx()).isEqualTo(widthPx);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(widthPx);
    }

    @Test
    public void testCreateAdapter_noContent() {
        Element model = asElement(ImageElement.getDefaultInstance());

        adapter.createAdapter(model, frameContext);

        assertThatRunnable(() -> adapter.bindModel(model, frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Unsupported or missing content");
    }

    @Test
    public void testBindModel_image() {
        StyleIdsStack styles = StyleIdsStack.newBuilder().addStyleIds("stylecat").build();
        Element model = Element.newBuilder()
                                .setStyleReferences(styles)
                                .setImageElement(ImageElement.newBuilder().setImage(DEFAULT_IMAGE))
                                .build();

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        imageView = adapter.getBaseView();
        verify(assetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, loadImageCallback);

        assertThat(adapter.getModel()).isSameInstanceAs(model.getImageElement());
        assertThat(adapter.getElementStyleIdsStack()).isEqualTo(styles);
    }

    @Test
    public void testBindModel_imageBinding() {
        ImageBindingRef imageBinding = ImageBindingRef.newBuilder().setBindingId("feline").build();
        Element model = asElement(ImageElement.newBuilder().setImageBinding(imageBinding).build());
        when(frameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.newBuilder().setImage(DEFAULT_IMAGE).build());

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        verify(assetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, loadImageCallback);
        assertThat(adapter.getModel()).isSameInstanceAs(model.getImageElement());
    }

    @Test
    public void testBindModel_optionalAbsent() {
        String bindingRef = "foto";
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(bindingRef).setIsOptional(true).build();
        Element imageBindingElement =
                asElement(ImageElement.newBuilder().setImageBinding(imageBindingRef).build());
        adapter.createAdapter(
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build()),
                frameContext);
        when(frameContext.getImageBindingValue(imageBindingRef))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.bindModel(imageBindingElement, frameContext);
        assertThat(adapter.getBaseView().getDrawable()).isNull();
        assertThat(adapter.getBaseView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBindModel_noContentInBindingValue() {
        String bindingRef = "foto";
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(bindingRef).build();
        Element imageBindingElement =
                asElement(ImageElement.newBuilder().setImageBinding(imageBindingRef).build());
        adapter.createAdapter(
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build()),
                frameContext);
        when(frameContext.getImageBindingValue(imageBindingRef))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(bindingRef)
                                    .setVisibility(Visibility.VISIBLE)
                                    .clearImage()
                                    .build());

        assertThatRunnable(() -> adapter.bindModel(imageBindingElement, frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Image binding foto had no content");
    }

    @Test
    public void testBindModel_setsScaleType() {
        StyleIdsStack styles = StyleIdsStack.newBuilder().addStyleIds("stylecat").build();
        when(styleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);
        Element model = asElement(ImageElement.newBuilder()
                                          .setImage(DEFAULT_IMAGE)
                                          .setStyleReferences(styles)
                                          .build());

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        verify(assetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, loadImageCallback);
        assertThat(adapter.scaleTypeForCallback).isEqualTo(ScaleType.CENTER_CROP);
    }

    @Test
    public void testBindModel_again() {
        // Bind a model, then unbind it.
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);
        imageView = adapter.getBaseView();
        RecyclerKey key1 = adapter.getKey();
        adapter.unbindModel();

        // Bind a different model
        Element model2 =
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build());
        adapter.bindModel(model2, frameContext);
        verify(assetProvider)
                .getImage(Image.getDefaultInstance(), WIDTH_DP, HEIGHT_DP, loadImageCallback);

        RecyclerKey key2 = adapter.getKey();
        assertThat(key1).isSameInstanceAs(key2);
        assertThat(adapter.getModel()).isSameInstanceAs(model2.getImageElement());
        assertThat(adapter.getView()).isNotNull();

        ImageView imageView2 = adapter.getBaseView();

        assertThat(imageView2).isSameInstanceAs(imageView);
    }

    @Test
    public void testBindModel_bindingTwiceThrowsException() {
        setStyle(HEIGHT_DP, WIDTH_DP);

        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);

        assertThatRunnable(() -> adapter.bindModel(DEFAULT_MODEL, frameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("An image loading callback exists");
    }

    @Test
    public void testBindModel_setsStylesOnlyIfBindingIsDefined() {
        // Create an adapter with a default style
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        verify(styleProvider).applyElementStyles(adapter);

        // Styles do not change when a different model is bound
        StyleIdsStack otherStyle = StyleIdsStack.newBuilder().addStyleIds("ignored").build();
        Element imageWithOtherStyle =
                DEFAULT_MODEL.toBuilder().setStyleReferences(otherStyle).build();
        adapter.bindModel(imageWithOtherStyle, frameContext);
        adapter.unbindModel();

        verify(frameContext, never()).makeStyleFor(otherStyle);

        // Styles do change when a model with a style binding is bound
        StyleIdsStack boundStyle =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("tuna"))
                        .build();
        Element imageWithBoundStyle =
                DEFAULT_MODEL.toBuilder().setStyleReferences(boundStyle).build();
        adapter.bindModel(imageWithBoundStyle, frameContext);

        verify(frameContext).makeStyleFor(boundStyle);

        verify(styleProvider, times(2)).applyElementStyles(adapter);
    }

    @Test
    public void testBindModel_preLoadFill() {
        Drawable preLoadFillDrawable = new ColorDrawable(Color.RED);

        // Set up the StyleProvider mock
        when(styleProvider.createPreLoadFill()).thenReturn(preLoadFillDrawable);
        when(styleProvider.hasPreLoadFill()).thenReturn(true);

        // Bind and expect the pre-load fill to be set
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);
        assertThat(adapter.getBaseView().getDrawable()).isSameInstanceAs(preLoadFillDrawable);

        // Load drawable and replace pre-load fill
        verify(assetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, loadImageCallback);
    }

    @Test
    public void testBindModel_color() {
        int red = 0xFFFF0000;
        ImageElement defaultImageElement =
                ImageElement.newBuilder().setImage(DEFAULT_IMAGE).build();
        StyleProvider redTintStyleProvider = new StyleProvider(
                Style.newBuilder().setStyleId("red").setColor(red).build(), assetProvider);
        StyleIdsStack redTintStyle = StyleIdsStack.newBuilder().addStyleIds("red").build();
        when(frameContext.makeStyleFor(redTintStyle)).thenReturn(redTintStyleProvider);

        Element modelWithOverlayColor = Element.newBuilder()
                                                .setStyleReferences(redTintStyle)
                                                .setImageElement(defaultImageElement)
                                                .build();

        // Bind and expect tint to be set
        adapter.createAdapter(modelWithOverlayColor, frameContext);
        adapter.bindModel(modelWithOverlayColor, frameContext);
        verify(frameContext).makeStyleFor(redTintStyle);
        assertThat(adapter.overlayColorForCallback).isEqualTo(red);
    }

    @Test
    public void testBindModel_filtersImageSources() {
        ImageSource activeSource =
                ImageSource.newBuilder()
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.DARK)))
                        .build();
        ImageSource inactiveSource =
                ImageSource.newBuilder()
                        .addConditions(MediaQueryCondition.newBuilder().setDarkLight(
                                DarkLightCondition.newBuilder().setMode(DarkLightMode.LIGHT)))
                        .build();
        Image image =
                Image.newBuilder().addSources(activeSource).addSources(inactiveSource).build();
        Image filteredImage = Image.newBuilder().addSources(activeSource).build();
        when(frameContext.filterImageSourcesByMediaQueryCondition(image)).thenReturn(filteredImage);

        Element model = asElement(ImageElement.newBuilder().setImage(image).build());

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        verify(assetProvider)
                .getImage(eq(filteredImage), anyInt(), anyInt(), any(LoadImageCallback.class));
    }

    @Test
    public void testBindModel_setsAspectRatio() {
        StyleIdsStack styles = StyleIdsStack.newBuilder().addStyleIds("stylecat").build();
        setStyle(null, null);
        Element model = asElement(ImageElement.newBuilder()
                                          .setImage(Image.newBuilder().addSources(
                                                  ImageSource.newBuilder()
                                                          .setWidthPx(100)
                                                          .setHeightPx(20) // Aspect ratio of 5.0
                                                          .setUrl("http://whatever")))
                                          .setStyleReferences(styles)
                                          .build());

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        imageView = adapter.getBaseView();

        imageView.measure(MeasureSpec.makeMeasureSpec(10, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertThat(imageView.getMeasuredWidth()).isEqualTo(10);
        assertThat(imageView.getMeasuredHeight()).isEqualTo(2);
    }

    @Test
    public void testUnbind() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);
        adapter.unbindModel();

        assertThat(adapter.getView()).isNotNull();
        assertThat(adapter.getBaseView().getDrawable()).isNull();

        assertThat(adapter.getComputedHeightPx()).isEqualTo(heightPx);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(widthPx);
    }

    @Test
    public void testUnbind_cancelsCallback() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);

        imageView = adapter.getBaseView();

        adapter.unbindModel();
        verify(assetProvider).getImage(DEFAULT_IMAGE, WIDTH_DP, HEIGHT_DP, loadImageCallback);
        verify(loadImageCallback).cancel();
    }

    @Test
    public void testReleaseAdapter_resetsDims() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);
        adapter.unbindModel();
        adapter.releaseAdapter();

        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testComputedDimensions_unbound() {
        assertThat(adapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testComputedDimensions_bound() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getComputedHeightPx()).isEqualTo(heightPx);
        assertThat(adapter.getComputedWidthPx()).isEqualTo(widthPx);
    }

    @Test
    public void testGetAspectRatio_succeeds() {
        Image image = Image.newBuilder()
                              .addSources(ImageSource.getDefaultInstance())
                              .addSources(ImageSource.newBuilder().setHeightPx(123))
                              .addSources(ImageSource.newBuilder().setWidthPx(456))
                              .addSources(ImageSource.newBuilder().setWidthPx(99).setHeightPx(
                                      33)) // This one gets picked
                              .addSources(ImageSource.newBuilder().setWidthPx(100).setHeightPx(50))
                              .build();
        assertThat(ImageElementAdapter.getAspectRatio(image)).isWithin(0.01f).of(3.0f);
    }

    @Test
    public void testGetAspectRatio_fails() {
        Image image = Image.newBuilder()
                              .addSources(ImageSource.getDefaultInstance())
                              .addSources(ImageSource.newBuilder().setHeightPx(123))
                              .addSources(ImageSource.newBuilder().setWidthPx(456))
                              .build();
        assertThat(ImageElementAdapter.getAspectRatio(image)).isZero();
        assertThat(ImageElementAdapter.getAspectRatio(Image.getDefaultInstance())).isZero();
    }

    @Test
    public void testGetModelFromElement() {
        ImageElement model =
                ImageElement.newBuilder()
                        .setStyleReferences(StyleIdsStack.newBuilder().addStyleIds("image"))
                        .build();

        Element elementWithModel = Element.newBuilder().setImageElement(model).build();
        assertThat(adapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> adapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ImageElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> adapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ImageElement");
    }

    private void setStyle(/*@Nullable*/ Integer height, /*@Nullable*/ Integer width) {
        if (height != null) {
            when(styleProvider.hasHeight()).thenReturn(true);
            when(styleProvider.getHeightSpecPx(context)).thenReturn(height);
        } else {
            when(styleProvider.hasHeight()).thenReturn(false);
            when(styleProvider.getHeightSpecPx(context)).thenReturn(DIMENSION_NOT_SET);
        }
        if (width != null) {
            when(styleProvider.hasWidth()).thenReturn(true);
            when(styleProvider.getWidthSpecPx(context)).thenReturn(width);
        } else {
            when(styleProvider.hasWidth()).thenReturn(false);
            when(styleProvider.getWidthSpecPx(context)).thenReturn(DIMENSION_NOT_SET);
        }
    }

    private static Element asElement(ImageElement imageElement) {
        return Element.newBuilder().setImageElement(imageElement).build();
    }

    private class ImageElementAdapterForTest extends ImageElementAdapter {
        private ScaleType scaleTypeForCallback;
        private Integer overlayColorForCallback;

        private ImageElementAdapterForTest(Context context, AdapterParameters parameters) {
            super(context, parameters);
        }

        @Override
        LoadImageCallback createLoadImageCallback(ScaleType scaleType,
                /*@Nullable*/ Integer overlayColor, FrameContext frameContext) {
            this.scaleTypeForCallback = scaleType;
            this.overlayColorForCallback = overlayColor;
            return loadImageCallback;
        }
    }
}

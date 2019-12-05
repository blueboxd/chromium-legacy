// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;
import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerWrapperView;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ImageBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.DarkLightCondition.DarkLightMode;
import org.chromium.components.feed.core.proto.ui.piet.MediaQueriesProto.MediaQueryCondition;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners.Corners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.EdgeWidths;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Style;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ImageElementAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private static final boolean LEGACY_CORNERS_FLAG = false;
    private static final boolean OUTLINE_CORNERS_FLAG = false;

    @Mock
    private ElementAdapterFactory mAdapterFactory;
    @Mock
    private TemplateBinder mTemplateBinder;
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private AssetProvider mAssetProvider;
    @Mock
    private StyleProvider mStyleProvider;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private LoadImageCallback mLoadImageCallback;

    private Context mContext;
    private int mHeightPx;
    private int mWidthPx;
    private ImageView mImageView;
    private final FakeClock mClock = new FakeClock();
    private RoundedCornerMaskCache mMaskCache;

    private ImageElementAdapterForTest mAdapter;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mHeightPx = (int) LayoutUtils.dpToPx(HEIGHT_DP, mContext);
        mWidthPx = (int) LayoutUtils.dpToPx(WIDTH_DP, mContext);
        mMaskCache = new RoundedCornerMaskCache();
        AdapterParameters parameters = new AdapterParameters(mContext, null, mHostProviders, null,
                mAdapterFactory, mTemplateBinder, mClock, new PietStylesHelperFactory(), mMaskCache,
                LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG);

        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(mStyleProvider);
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);
        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mStyleProvider.getPadding()).thenReturn(PADDING);
        when(mStyleProvider.hasRoundedCorners()).thenReturn(true);
        when(mStyleProvider.getRoundedCorners()).thenReturn(CORNERS);
        when(mStyleProvider.getScaleType()).thenReturn(ScaleType.FIT_CENTER);
        when(mStyleProvider.createWrapperView(
                     mContext, mMaskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(new RoundedCornerWrapperView(mContext, CORNERS, mMaskCache,
                        Suppliers.of(false),
                        /*radiusOverride= */ 0,
                        /* borders= */ null,
                        /* allowClipPath= */ false,
                        /* allowOutlineRounding= */ false));
        setStyle(null, null);

        mAdapter = new ImageElementAdapterForTest(mContext, parameters);
    }

    @Test
    public void testCreate() {
        assertThat(mAdapter).isNotNull();
    }

    @Test
    public void testCreateAdapter() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        assertThat(mAdapter.getModel()).isSameInstanceAs(DEFAULT_MODEL.getImageElement());

        assertThat(mAdapter.getView()).isNotNull();

        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(mHeightPx);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(mWidthPx);
        assertThat(mAdapter.getBaseView().getCropToPadding()).isTrue();
        verify(mStyleProvider).applyElementStyles(mAdapter);
    }

    @Test
    public void testCreateAdapter_noDimensionsSet() {
        setStyle(null, null);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getModel()).isSameInstanceAs(DEFAULT_MODEL.getImageElement());

        assertThat(mAdapter.getView()).isNotNull();

        // Assert that width and height are set to the defaults
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testCreateAdapter_heightOnly() {
        setStyle(HEIGHT_DP, null);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getModel()).isEqualTo(DEFAULT_MODEL.getImageElement());

        assertThat(mAdapter.getView()).isNotNull();

        // Width defaults to MATCH_PARENT
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(mHeightPx);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testCreateAdapter_widthOnly() {
        setStyle(null, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getModel()).isEqualTo(DEFAULT_MODEL.getImageElement());

        assertThat(mAdapter.getView()).isNotNull();

        // Image defaults to a square.
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(mWidthPx);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(mWidthPx);
    }

    @Test
    public void testCreateAdapter_noContent() {
        Element model = asElement(ImageElement.getDefaultInstance());

        mAdapter.createAdapter(model, mFrameContext);

        assertThatRunnable(() -> mAdapter.bindModel(model, mFrameContext))
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

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        mImageView = mAdapter.getBaseView();
        verify(mAssetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, mLoadImageCallback);

        assertThat(mAdapter.getModel()).isSameInstanceAs(model.getImageElement());
        assertThat(mAdapter.getElementStyleIdsStack()).isEqualTo(styles);
    }

    @Test
    public void testBindModel_imageBinding() {
        ImageBindingRef imageBinding = ImageBindingRef.newBuilder().setBindingId("feline").build();
        Element model = asElement(ImageElement.newBuilder().setImageBinding(imageBinding).build());
        when(mFrameContext.getImageBindingValue(imageBinding))
                .thenReturn(BindingValue.newBuilder().setImage(DEFAULT_IMAGE).build());

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        verify(mAssetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, mLoadImageCallback);
        assertThat(mAdapter.getModel()).isSameInstanceAs(model.getImageElement());
    }

    @Test
    public void testBindModel_optionalAbsent() {
        String bindingRef = "foto";
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(bindingRef).setIsOptional(true).build();
        Element imageBindingElement =
                asElement(ImageElement.newBuilder().setImageBinding(imageBindingRef).build());
        mAdapter.createAdapter(
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build()),
                mFrameContext);
        when(mFrameContext.getImageBindingValue(imageBindingRef))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.bindModel(imageBindingElement, mFrameContext);
        assertThat(mAdapter.getBaseView().getDrawable()).isNull();
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBindModel_noContentInBindingValue() {
        String bindingRef = "foto";
        ImageBindingRef imageBindingRef =
                ImageBindingRef.newBuilder().setBindingId(bindingRef).build();
        Element imageBindingElement =
                asElement(ImageElement.newBuilder().setImageBinding(imageBindingRef).build());
        mAdapter.createAdapter(
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build()),
                mFrameContext);
        when(mFrameContext.getImageBindingValue(imageBindingRef))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(bindingRef)
                                    .setVisibility(Visibility.VISIBLE)
                                    .clearImage()
                                    .build());

        assertThatRunnable(() -> mAdapter.bindModel(imageBindingElement, mFrameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Image binding foto had no content");
    }

    @Test
    public void testBindModel_setsScaleType() {
        StyleIdsStack styles = StyleIdsStack.newBuilder().addStyleIds("stylecat").build();
        when(mStyleProvider.getScaleType()).thenReturn(ImageView.ScaleType.CENTER_CROP);
        Element model = asElement(ImageElement.newBuilder()
                                          .setImage(DEFAULT_IMAGE)
                                          .setStyleReferences(styles)
                                          .build());

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        verify(mAssetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, mLoadImageCallback);
        assertThat(mAdapter.mScaleTypeForCallback).isEqualTo(ScaleType.CENTER_CROP);
    }

    @Test
    public void testBindModel_again() {
        // Bind a model, then unbind it.
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);
        mImageView = mAdapter.getBaseView();
        RecyclerKey key1 = mAdapter.getKey();
        mAdapter.unbindModel();

        // Bind a different model
        Element model2 =
                asElement(ImageElement.newBuilder().setImage(Image.getDefaultInstance()).build());
        mAdapter.bindModel(model2, mFrameContext);
        verify(mAssetProvider)
                .getImage(Image.getDefaultInstance(), WIDTH_DP, HEIGHT_DP, mLoadImageCallback);

        RecyclerKey key2 = mAdapter.getKey();
        assertThat(key1).isSameInstanceAs(key2);
        assertThat(mAdapter.getModel()).isSameInstanceAs(model2.getImageElement());
        assertThat(mAdapter.getView()).isNotNull();

        ImageView imageView2 = mAdapter.getBaseView();

        assertThat(imageView2).isSameInstanceAs(mImageView);
    }

    @Test
    public void testBindModel_bindingTwiceThrowsException() {
        setStyle(HEIGHT_DP, WIDTH_DP);

        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);

        assertThatRunnable(() -> mAdapter.bindModel(DEFAULT_MODEL, mFrameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("An image loading callback exists");
    }

    @Test
    public void testBindModel_setsStylesOnlyIfBindingIsDefined() {
        // Create an adapter with a default style
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        verify(mStyleProvider).applyElementStyles(mAdapter);

        // Styles do not change when a different model is bound
        StyleIdsStack otherStyle = StyleIdsStack.newBuilder().addStyleIds("ignored").build();
        Element imageWithOtherStyle =
                DEFAULT_MODEL.toBuilder().setStyleReferences(otherStyle).build();
        mAdapter.bindModel(imageWithOtherStyle, mFrameContext);
        mAdapter.unbindModel();

        verify(mFrameContext, never()).makeStyleFor(otherStyle);

        // Styles do change when a model with a style binding is bound
        StyleIdsStack boundStyle =
                StyleIdsStack.newBuilder()
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("tuna"))
                        .build();
        Element imageWithBoundStyle =
                DEFAULT_MODEL.toBuilder().setStyleReferences(boundStyle).build();
        mAdapter.bindModel(imageWithBoundStyle, mFrameContext);

        verify(mFrameContext).makeStyleFor(boundStyle);

        verify(mStyleProvider, times(2)).applyElementStyles(mAdapter);
    }

    @Test
    public void testBindModel_preLoadFill() {
        Drawable preLoadFillDrawable = new ColorDrawable(Color.RED);

        // Set up the StyleProvider mock
        when(mStyleProvider.createPreLoadFill()).thenReturn(preLoadFillDrawable);
        when(mStyleProvider.hasPreLoadFill()).thenReturn(true);

        // Bind and expect the pre-load fill to be set
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);
        assertThat(mAdapter.getBaseView().getDrawable()).isSameInstanceAs(preLoadFillDrawable);

        // Load drawable and replace pre-load fill
        verify(mAssetProvider)
                .getImage(DEFAULT_IMAGE, DIMENSION_UNKNOWN, DIMENSION_UNKNOWN, mLoadImageCallback);
    }

    @Test
    public void testBindModel_color() {
        int red = 0xFFFF0000;
        ImageElement defaultImageElement =
                ImageElement.newBuilder().setImage(DEFAULT_IMAGE).build();
        StyleProvider redTintStyleProvider = new StyleProvider(
                Style.newBuilder().setStyleId("red").setColor(red).build(), mAssetProvider);
        StyleIdsStack redTintStyle = StyleIdsStack.newBuilder().addStyleIds("red").build();
        when(mFrameContext.makeStyleFor(redTintStyle)).thenReturn(redTintStyleProvider);

        Element modelWithOverlayColor = Element.newBuilder()
                                                .setStyleReferences(redTintStyle)
                                                .setImageElement(defaultImageElement)
                                                .build();

        // Bind and expect tint to be set
        mAdapter.createAdapter(modelWithOverlayColor, mFrameContext);
        mAdapter.bindModel(modelWithOverlayColor, mFrameContext);
        verify(mFrameContext).makeStyleFor(redTintStyle);
        assertThat(mAdapter.mOverlayColorForCallback).isEqualTo(red);
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
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(image))
                .thenReturn(filteredImage);

        Element model = asElement(ImageElement.newBuilder().setImage(image).build());

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        verify(mAssetProvider)
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

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        mImageView = mAdapter.getBaseView();

        mImageView.measure(MeasureSpec.makeMeasureSpec(10, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        assertThat(mImageView.getMeasuredWidth()).isEqualTo(10);
        assertThat(mImageView.getMeasuredHeight()).isEqualTo(2);
    }

    @Test
    public void testUnbind() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);
        mAdapter.unbindModel();

        assertThat(mAdapter.getView()).isNotNull();
        assertThat(mAdapter.getBaseView().getDrawable()).isNull();

        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(mHeightPx);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(mWidthPx);
    }

    @Test
    public void testUnbind_cancelsCallback() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);

        mImageView = mAdapter.getBaseView();

        mAdapter.unbindModel();
        verify(mAssetProvider).getImage(DEFAULT_IMAGE, WIDTH_DP, HEIGHT_DP, mLoadImageCallback);
        verify(mLoadImageCallback).cancel();
    }

    @Test
    public void testReleaseAdapter_resetsDims() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testComputedDimensions_unbound() {
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void testComputedDimensions_bound() {
        setStyle(HEIGHT_DP, WIDTH_DP);
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(mHeightPx);
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(mWidthPx);
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
        assertThat(mAdapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> mAdapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ImageElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> mAdapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ImageElement");
    }

    private void setStyle(/*@Nullable*/ Integer height, /*@Nullable*/ Integer width) {
        if (height != null) {
            when(mStyleProvider.hasHeight()).thenReturn(true);
            when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(height);
        } else {
            when(mStyleProvider.hasHeight()).thenReturn(false);
            when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(DIMENSION_NOT_SET);
        }
        if (width != null) {
            when(mStyleProvider.hasWidth()).thenReturn(true);
            when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(width);
        } else {
            when(mStyleProvider.hasWidth()).thenReturn(false);
            when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(DIMENSION_NOT_SET);
        }
    }

    private static Element asElement(ImageElement imageElement) {
        return Element.newBuilder().setImageElement(imageElement).build();
    }

    private class ImageElementAdapterForTest extends ImageElementAdapter {
        private ScaleType mScaleTypeForCallback;
        private Integer mOverlayColorForCallback;

        private ImageElementAdapterForTest(Context context, AdapterParameters parameters) {
            super(context, parameters);
        }

        @Override
        LoadImageCallback createLoadImageCallback(ScaleType scaleType,
                /*@Nullable*/ Integer overlayColor, FrameContext frameContext) {
            this.mScaleTypeForCallback = scaleType;
            this.mOverlayColorForCallback = overlayColor;
            return mLoadImageCallback;
        }
    }
}

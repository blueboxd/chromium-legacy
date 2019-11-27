// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi.DIMENSION_UNKNOWN;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.SingletonKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.ui.AspectRatioScalingImageView;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;

/** An {@link ElementAdapter} for {@code ImageElement} elements. */
class ImageElementAdapter extends ElementAdapter<AspectRatioScalingImageView, ImageElement> {
    private static final String TAG = "ImageElementAdapter";

    /*@Nullable*/ private LoadImageCallback mCurrentlyLoadingImage;

    @VisibleForTesting
    ImageElementAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, createView(context), KeySupplier.SINGLETON_KEY);
    }

    @Override
    ImageElement getModelFromElement(Element baseElement) {
        if (!baseElement.hasImageElement()) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing ImageElement; has %s", baseElement.getElementsCase()));
        }
        return baseElement.getImageElement();
    }

    @Override
    void onCreateAdapter(ImageElement model, Element baseElement, FrameContext frameContext) {
        StyleProvider style = getElementStyle();
        Context context = getContext();

        mWidthPx = style.getWidthSpecPx(context);

        if (style.hasHeight()) {
            mHeightPx = style.getHeightSpecPx(context);
        } else {
            // Defaults to a square when only the width is defined.
            // TODO: This is not cross-platform standard; should probably get rid of this.
            mHeightPx = mWidthPx > 0 ? mWidthPx : StyleProvider.DIMENSION_NOT_SET;
        }
    }

    @Override
    void onBindModel(ImageElement model, Element baseElement, FrameContext frameContext) {
        Image image;
        switch (model.getContentCase()) {
            case IMAGE:
                image = model.getImage();
                break;
            case IMAGE_BINDING:
                BindingValue binding = frameContext.getImageBindingValue(model.getImageBinding());
                if (!binding.hasImage()) {
                    if (model.getImageBinding().getIsOptional()) {
                        setVisibilityOnView(Visibility.GONE);
                        return;
                    } else {
                        throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                String.format(
                                        "Image binding %s had no content", binding.getBindingId()));
                    }
                }
                image = binding.getImage();
                break;
            default:
                throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        String.format("Unsupported or missing content in ImageElement: %s",
                                model.getContentCase()));
        }

        if (getElementStyle().hasPreLoadFill()) {
            getBaseView().setImageDrawable(getElementStyle().createPreLoadFill());
        }

        image = frameContext.filterImageSourcesByMediaQueryCondition(image);

        getBaseView().setDefaultAspectRatio(getAspectRatio(image));

        checkState(
                mCurrentlyLoadingImage == null, "An image loading callback exists; unbind first");
        Integer overlayColor = getElementStyle().hasColor() ? getElementStyle().getColor() : null;
        LoadImageCallback loadImageCallback = createLoadImageCallback(
                getElementStyle().getScaleType(), overlayColor, frameContext);
        mCurrentlyLoadingImage = loadImageCallback;
        getParameters().mHostProviders.getAssetProvider().getImage(image,
                convertDimensionForImageLoader(mWidthPx), convertDimensionForImageLoader(mHeightPx),
                loadImageCallback);
    }

    private int convertDimensionForImageLoader(int dimension) {
        return dimension < 0 ? DIMENSION_UNKNOWN : dimension;
    }

    @VisibleForTesting
    static float getAspectRatio(Image image) {
        for (ImageSource source : image.getSourcesList()) {
            if (source.getHeightPx() > 0 && source.getWidthPx() > 0) {
                return ((float) source.getWidthPx()) / source.getHeightPx();
            }
        }
        return 0.0f;
    }

    @Override
    void onUnbindModel() {
        if (mCurrentlyLoadingImage != null) {
            mCurrentlyLoadingImage.cancel();
            mCurrentlyLoadingImage = null;
        }
        ImageView imageView = getBaseView();
        if (imageView != null) {
            imageView.setImageDrawable(null);
        }
    }

    @VisibleForTesting
    LoadImageCallback createLoadImageCallback(
            ScaleType scaleType, /*@Nullable*/ Integer overlayColor, FrameContext frameContext) {
        return new LoadImageCallback(getBaseView(), scaleType, overlayColor,
                getElementStyle().getFadeInImageOnLoad(), getParameters(), frameContext);
    }

    private static AspectRatioScalingImageView createView(Context context) {
        AspectRatioScalingImageView imageView = new AspectRatioScalingImageView(context);
        imageView.setCropToPadding(true);
        return imageView;
    }

    static class KeySupplier extends SingletonKeySupplier<ImageElementAdapter, ImageElement> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public ImageElementAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new ImageElementAdapter(context, parameters);
        }
    }
}

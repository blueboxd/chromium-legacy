// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.SingletonKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

/** Adapter that manages a custom view created by the host. */
class CustomElementAdapter extends ElementAdapter<FrameLayout, CustomElement> {
    private static final String TAG = "CustomElementAdapter";
    CustomElementData mBoundCustomElementData = CustomElementData.getDefaultInstance();

    CustomElementAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, new FrameLayout(context), KeySupplier.SINGLETON_KEY);
    }

    @Override
    protected CustomElement getModelFromElement(Element baseElement) {
        if (!baseElement.hasCustomElement()) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing CustomElement; has %s", baseElement.getElementsCase()));
        }
        return baseElement.getCustomElement();
    }

    @Override
    void onBindModel(CustomElement customElement, Element baseElement, FrameContext frameContext) {
        switch (customElement.getContentCase()) {
            case CUSTOM_ELEMENT_DATA:
                mBoundCustomElementData = customElement.getCustomElementData();
                break;
            case CUSTOM_BINDING:
                BindingValue binding =
                        frameContext.getCustomElementBindingValue(customElement.getCustomBinding());
                if (!binding.hasCustomElementData()) {
                    if (customElement.getCustomBinding().getIsOptional()) {
                        setVisibilityOnView(Visibility.GONE);
                        return;
                    } else {
                        throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                frameContext.reportMessage(MessageType.ERROR,
                                        ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                        String.format("Custom element binding %s had no content",
                                                binding.getBindingId())));
                    }
                }
                mBoundCustomElementData = binding.getCustomElementData();
                break;
            default:
                Logger.e(TAG,
                        frameContext.reportMessage(MessageType.ERROR,
                                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                "Missing payload in CustomElement"));
                return;
        }

        View v = getParameters().mHostProviders.getCustomElementProvider().createCustomElement(
                mBoundCustomElementData);
        getBaseView().addView(v);
    }

    @Override
    void onUnbindModel() {
        FrameLayout baseView = getBaseView();
        // There should be a maximum of one child that was bound, so using the CustomElementData
        // that was saved during the last bind should be fine.
        if (baseView != null && baseView.getChildCount() > 0) {
            for (int i = 0; i < baseView.getChildCount(); i++) {
                getParameters().mHostProviders.getCustomElementProvider().releaseCustomView(
                        baseView.getChildAt(i), mBoundCustomElementData);
            }
            baseView.removeAllViews();
        }
    }

    static class KeySupplier extends SingletonKeySupplier<CustomElementAdapter, CustomElement> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public CustomElementAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new CustomElementAdapter(context, parameters);
        }
    }
}

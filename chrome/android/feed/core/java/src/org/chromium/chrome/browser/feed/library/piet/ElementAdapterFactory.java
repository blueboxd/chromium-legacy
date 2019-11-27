// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;

import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateKey;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Provides methods to create various adapter types based on bindings. */
class ElementAdapterFactory {
    private final AdapterFactory<CustomElementAdapter, CustomElement> mCustomElementFactory;
    private final AdapterFactory<ChunkedTextElementAdapter, Element> mChunkedTextElementFactory;
    private final AdapterFactory<ParameterizedTextElementAdapter, Element>
            mParameterizedTextElementFactory;
    private final AdapterFactory<ImageElementAdapter, ImageElement> mImageElementFactory;
    private final AdapterFactory<GridRowAdapter, GridRow> mGridRowFactory;
    private final AdapterFactory<ElementListAdapter, ElementList> mElementListFactory;
    private final AdapterFactory<ElementStackAdapter, ElementStack> mElementStackFactory;
    private final List<AdapterFactory<?, ?>> mFactories;

    private final KeyedRecyclerPool<ElementAdapter<? extends View, ?>> mTemplateRecyclerPool;

    ElementAdapterFactory(Context context, AdapterParameters parameters,
            KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool) {
        this(new AdapterFactory<>(context, parameters, new CustomElementAdapter.KeySupplier()),
                new AdapterFactory<>(
                        context, parameters, new ChunkedTextElementAdapter.KeySupplier()),
                new AdapterFactory<>(
                        context, parameters, new ParameterizedTextElementAdapter.KeySupplier()),
                new AdapterFactory<>(context, parameters, new ImageElementAdapter.KeySupplier()),
                new AdapterFactory<>(context, parameters, new GridRowAdapter.KeySupplier()),
                new AdapterFactory<>(context, parameters, new ElementListAdapter.KeySupplier()),
                new AdapterFactory<>(context, parameters, new ElementStackAdapter.KeySupplier()),
                templateRecyclerPool);
    }

    /** Testing-only constructor for mocking the factories. */
    @VisibleForTesting
    ElementAdapterFactory(AdapterFactory<CustomElementAdapter, CustomElement> customElementFactory,
            AdapterFactory<ChunkedTextElementAdapter, Element> chunkedTextElementFactory,
            AdapterFactory<ParameterizedTextElementAdapter, Element>
                    parameterizedTextElementFactory,
            AdapterFactory<ImageElementAdapter, ImageElement> imageElementFactory,
            AdapterFactory<GridRowAdapter, GridRow> gridRowFactory,
            AdapterFactory<ElementListAdapter, ElementList> elementListFactory,
            AdapterFactory<ElementStackAdapter, ElementStack> elementStackFactory,
            KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool) {
        this.mCustomElementFactory = customElementFactory;
        this.mChunkedTextElementFactory = chunkedTextElementFactory;
        this.mParameterizedTextElementFactory = parameterizedTextElementFactory;
        this.mImageElementFactory = imageElementFactory;
        this.mGridRowFactory = gridRowFactory;
        this.mElementListFactory = elementListFactory;
        this.mElementStackFactory = elementStackFactory;
        mFactories = Collections.unmodifiableList(Arrays.asList(customElementFactory,
                chunkedTextElementFactory, parameterizedTextElementFactory, imageElementFactory,
                gridRowFactory, elementListFactory, elementStackFactory));
        this.mTemplateRecyclerPool = templateRecyclerPool;
    }

    ElementAdapter<? extends View, ?> createAdapterForElement(
            Element element, FrameContext frameContext) {
        ElementAdapter<? extends View, ?> returnAdapter;
        switch (element.getElementsCase()) {
            case CUSTOM_ELEMENT:
                returnAdapter = mCustomElementFactory.get(element.getCustomElement(), frameContext);
                break;
            case TEXT_ELEMENT:
                switch (element.getTextElement().getContentCase()) {
                    case CHUNKED_TEXT:
                    case CHUNKED_TEXT_BINDING:
                        returnAdapter = mChunkedTextElementFactory.get(element, frameContext);
                        break;
                    case PARAMETERIZED_TEXT:
                    case PARAMETERIZED_TEXT_BINDING:
                        returnAdapter = mParameterizedTextElementFactory.get(element, frameContext);
                        break;
                    default:
                        throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                frameContext.reportMessage(MessageType.ERROR,
                                        ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                        String.format("Unsupported TextElement type: %s",
                                                element.getTextElement().getContentCase())));
                }
                break;
            case IMAGE_ELEMENT:
                returnAdapter = mImageElementFactory.get(element.getImageElement(), frameContext);
                break;
            case GRID_ROW:
                returnAdapter = mGridRowFactory.get(element.getGridRow(), frameContext);
                break;
            case ELEMENT_LIST:
                returnAdapter = mElementListFactory.get(element.getElementList(), frameContext);
                break;
            case ELEMENT_STACK:
                returnAdapter = mElementStackFactory.get(element.getElementStack(), frameContext);
                break;
            case ELEMENTS_NOT_SET:
            default:
                throw new PietFatalException(ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        frameContext.reportMessage(MessageType.ERROR,
                                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                                String.format("Unsupported Element type: %s",
                                        element.getElementsCase())));
        }
        returnAdapter.createAdapter(element, frameContext);
        return returnAdapter;
    }

    void releaseAdapter(ElementAdapter<? extends View, ?> adapter) {
        adapter.unbindModel();

        if (adapter.getKey() instanceof TemplateKey) {
            // Don't release template adapters; just return them to the pool after unbinding.
            TemplateKey key = (TemplateKey) adapter.getKey();
            if (key != null) {
                mTemplateRecyclerPool.put(key, adapter);
            }
        } else if (adapter instanceof CustomElementAdapter) {
            mCustomElementFactory.release((CustomElementAdapter) adapter);
        } else if (adapter instanceof ChunkedTextElementAdapter) {
            mChunkedTextElementFactory.release((ChunkedTextElementAdapter) adapter);
        } else if (adapter instanceof ParameterizedTextElementAdapter) {
            mParameterizedTextElementFactory.release((ParameterizedTextElementAdapter) adapter);
        } else if (adapter instanceof ImageElementAdapter) {
            mImageElementFactory.release((ImageElementAdapter) adapter);
        } else if (adapter instanceof GridRowAdapter) {
            mGridRowFactory.release((GridRowAdapter) adapter);
        } else if (adapter instanceof ElementListAdapter) {
            mElementListFactory.release((ElementListAdapter) adapter);
        } else if (adapter instanceof ElementStackAdapter) {
            mElementStackFactory.release((ElementStackAdapter) adapter);
        }
    }

    void purgeRecyclerPools() {
        for (AdapterFactory<?, ?> factory : mFactories) {
            factory.purgeRecyclerPool();
        }
        mTemplateRecyclerPool.clear();
    }
}

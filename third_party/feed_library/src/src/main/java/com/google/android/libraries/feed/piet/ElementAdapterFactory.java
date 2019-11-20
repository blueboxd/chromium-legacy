// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.android.libraries.feed.piet.TemplateBinder.TemplateKey;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.ElementStack;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Provides methods to create various adapter types based on bindings. */
class ElementAdapterFactory {
  private final AdapterFactory<CustomElementAdapter, CustomElement> customElementFactory;
  private final AdapterFactory<ChunkedTextElementAdapter, Element> chunkedTextElementFactory;
  private final AdapterFactory<ParameterizedTextElementAdapter, Element>
      parameterizedTextElementFactory;
  private final AdapterFactory<ImageElementAdapter, ImageElement> imageElementFactory;
  private final AdapterFactory<GridRowAdapter, GridRow> gridRowFactory;
  private final AdapterFactory<ElementListAdapter, ElementList> elementListFactory;
  private final AdapterFactory<ElementStackAdapter, ElementStack> elementStackFactory;
  private final List<AdapterFactory<?, ?>> factories;

  private final KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool;

  ElementAdapterFactory(
      Context context,
      AdapterParameters parameters,
      KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool) {
    this(
        new AdapterFactory<>(context, parameters, new CustomElementAdapter.KeySupplier()),
        new AdapterFactory<>(context, parameters, new ChunkedTextElementAdapter.KeySupplier()),
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
  ElementAdapterFactory(
      AdapterFactory<CustomElementAdapter, CustomElement> customElementFactory,
      AdapterFactory<ChunkedTextElementAdapter, Element> chunkedTextElementFactory,
      AdapterFactory<ParameterizedTextElementAdapter, Element> parameterizedTextElementFactory,
      AdapterFactory<ImageElementAdapter, ImageElement> imageElementFactory,
      AdapterFactory<GridRowAdapter, GridRow> gridRowFactory,
      AdapterFactory<ElementListAdapter, ElementList> elementListFactory,
      AdapterFactory<ElementStackAdapter, ElementStack> elementStackFactory,
      KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool) {
    this.customElementFactory = customElementFactory;
    this.chunkedTextElementFactory = chunkedTextElementFactory;
    this.parameterizedTextElementFactory = parameterizedTextElementFactory;
    this.imageElementFactory = imageElementFactory;
    this.gridRowFactory = gridRowFactory;
    this.elementListFactory = elementListFactory;
    this.elementStackFactory = elementStackFactory;
    factories =
        Collections.unmodifiableList(
            Arrays.asList(
                customElementFactory,
                chunkedTextElementFactory,
                parameterizedTextElementFactory,
                imageElementFactory,
                gridRowFactory,
                elementListFactory,
                elementStackFactory));
    this.templateRecyclerPool = templateRecyclerPool;
  }

  ElementAdapter<? extends View, ?> createAdapterForElement(
      Element element, FrameContext frameContext) {
    ElementAdapter<? extends View, ?> returnAdapter;
    switch (element.getElementsCase()) {
      case CUSTOM_ELEMENT:
        returnAdapter = customElementFactory.get(element.getCustomElement(), frameContext);
        break;
      case TEXT_ELEMENT:
        switch (element.getTextElement().getContentCase()) {
          case CHUNKED_TEXT:
          case CHUNKED_TEXT_BINDING:
            returnAdapter = chunkedTextElementFactory.get(element, frameContext);
            break;
          case PARAMETERIZED_TEXT:
          case PARAMETERIZED_TEXT_BINDING:
            returnAdapter = parameterizedTextElementFactory.get(element, frameContext);
            break;
          default:
            throw new PietFatalException(
                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                frameContext.reportMessage(
                    MessageType.ERROR,
                    ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                    String.format(
                        "Unsupported TextElement type: %s",
                        element.getTextElement().getContentCase())));
        }
        break;
      case IMAGE_ELEMENT:
        returnAdapter = imageElementFactory.get(element.getImageElement(), frameContext);
        break;
      case GRID_ROW:
        returnAdapter = gridRowFactory.get(element.getGridRow(), frameContext);
        break;
      case ELEMENT_LIST:
        returnAdapter = elementListFactory.get(element.getElementList(), frameContext);
        break;
      case ELEMENT_STACK:
        returnAdapter = elementStackFactory.get(element.getElementStack(), frameContext);
        break;
      case ELEMENTS_NOT_SET:
      default:
        throw new PietFatalException(
            ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
            frameContext.reportMessage(
                MessageType.ERROR,
                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                String.format("Unsupported Element type: %s", element.getElementsCase())));
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
        templateRecyclerPool.put(key, adapter);
      }
    } else if (adapter instanceof CustomElementAdapter) {
      customElementFactory.release((CustomElementAdapter) adapter);
    } else if (adapter instanceof ChunkedTextElementAdapter) {
      chunkedTextElementFactory.release((ChunkedTextElementAdapter) adapter);
    } else if (adapter instanceof ParameterizedTextElementAdapter) {
      parameterizedTextElementFactory.release((ParameterizedTextElementAdapter) adapter);
    } else if (adapter instanceof ImageElementAdapter) {
      imageElementFactory.release((ImageElementAdapter) adapter);
    } else if (adapter instanceof GridRowAdapter) {
      gridRowFactory.release((GridRowAdapter) adapter);
    } else if (adapter instanceof ElementListAdapter) {
      elementListFactory.release((ElementListAdapter) adapter);
    } else if (adapter instanceof ElementStackAdapter) {
      elementStackFactory.release((ElementStackAdapter) adapter);
    }
  }

  void purgeRecyclerPools() {
    for (AdapterFactory<?, ?> factory : factories) {
      factory.purgeRecyclerPool();
    }
    templateRecyclerPool.clear();
  }
}

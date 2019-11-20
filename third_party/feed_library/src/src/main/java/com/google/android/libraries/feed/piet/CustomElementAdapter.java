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
import android.view.View;
import android.widget.FrameLayout;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.piet.AdapterFactory.SingletonKeySupplier;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.CustomElementData;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

/** Adapter that manages a custom view created by the host. */
class CustomElementAdapter extends ElementAdapter<FrameLayout, CustomElement> {
  private static final String TAG = "CustomElementAdapter";
  CustomElementData boundCustomElementData = CustomElementData.getDefaultInstance();

  CustomElementAdapter(Context context, AdapterParameters parameters) {
    super(context, parameters, new FrameLayout(context), KeySupplier.SINGLETON_KEY);
  }

  @Override
  protected CustomElement getModelFromElement(Element baseElement) {
    if (!baseElement.hasCustomElement()) {
      throw new PietFatalException(
          ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
          String.format("Missing CustomElement; has %s", baseElement.getElementsCase()));
    }
    return baseElement.getCustomElement();
  }

  @Override
  void onBindModel(CustomElement customElement, Element baseElement, FrameContext frameContext) {
    switch (customElement.getContentCase()) {
      case CUSTOM_ELEMENT_DATA:
        boundCustomElementData = customElement.getCustomElementData();
        break;
      case CUSTOM_BINDING:
        BindingValue binding =
            frameContext.getCustomElementBindingValue(customElement.getCustomBinding());
        if (!binding.hasCustomElementData()) {
          if (customElement.getCustomBinding().getIsOptional()) {
            setVisibilityOnView(Visibility.GONE);
            return;
          } else {
            throw new PietFatalException(
                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                frameContext.reportMessage(
                    MessageType.ERROR,
                    ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                    String.format(
                        "Custom element binding %s had no content", binding.getBindingId())));
          }
        }
        boundCustomElementData = binding.getCustomElementData();
        break;
      default:
        Logger.e(
            TAG,
            frameContext.reportMessage(
                MessageType.ERROR,
                ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                "Missing payload in CustomElement"));
        return;
    }

    View v =
        getParameters()
            .hostProviders
            .getCustomElementProvider()
            .createCustomElement(boundCustomElementData);
    getBaseView().addView(v);
  }

  @Override
  void onUnbindModel() {
    FrameLayout baseView = getBaseView();
    // There should be a maximum of one child that was bound, so using the CustomElementData that
    // was saved during the last bind should be fine.
    if (baseView != null && baseView.getChildCount() > 0) {
      for (int i = 0; i < baseView.getChildCount(); i++) {
        getParameters()
            .hostProviders
            .getCustomElementProvider()
            .releaseCustomView(baseView.getChildAt(i), boundCustomElementData);
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

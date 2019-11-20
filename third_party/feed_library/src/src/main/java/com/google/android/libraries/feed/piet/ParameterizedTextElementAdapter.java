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
import android.widget.TextView;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

/** An {@link ElementAdapter} which manages {@code ParameterizedText} elements. */
class ParameterizedTextElementAdapter extends TextElementAdapter {
  private static final String TAG = "ParameterizedTextElementAdapter";

  ParameterizedTextElementAdapter(Context context, AdapterParameters parameters) {
    super(context, parameters);
  }

  @Override
  void setTextOnView(FrameContext frameContext, TextElement textLine) {
    switch (textLine.getContentCase()) {
      case PARAMETERIZED_TEXT:
        // No bindings found, so use the inlined value (or empty if not set)
        setTextOnView(getBaseView(), textLine.getParameterizedText());
        break;
      case PARAMETERIZED_TEXT_BINDING:
        BindingValue bindingValue =
            frameContext.getParameterizedTextBindingValue(textLine.getParameterizedTextBinding());

        if (!bindingValue.hasParameterizedText()
            && !textLine.getParameterizedTextBinding().getIsOptional()) {
          throw new PietFatalException(
              ErrorCode.ERR_MISSING_BINDING_VALUE,
              String.format(
                  "Parameterized text binding %s had no content", bindingValue.getBindingId()));
        }

        setTextOnView(getBaseView(), bindingValue.getParameterizedText());
        break;
      default:
        frameContext.reportMessage(
            MessageType.ERROR,
            ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
            String.format(
                "TextElement missing ParameterizedText content; has %s",
                textLine.getContentCase()));
        setTextOnView(getBaseView(), ParameterizedText.getDefaultInstance());
    }
  }

  private void setTextOnView(TextView textView, ParameterizedText parameterizedText) {
    if (!parameterizedText.hasText()) {
      textView.setText("");
      return;
    }

    textView.setText(
        getTemplatedStringEvaluator()
            .evaluate(getParameters().hostProviders.getAssetProvider(), parameterizedText));
  }

  static class KeySupplier extends TextElementKeySupplier<ParameterizedTextElementAdapter> {
    @Override
    public String getAdapterTag() {
      return TAG;
    }

    @Override
    public ParameterizedTextElementAdapter getAdapter(
        Context context, AdapterParameters parameters) {
      return new ParameterizedTextElementAdapter(context, parameters);
    }
  }
}

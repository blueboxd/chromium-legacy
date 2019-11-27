// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.widget.TextView;

import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;

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
                BindingValue bindingValue = frameContext.getParameterizedTextBindingValue(
                        textLine.getParameterizedTextBinding());

                if (!bindingValue.hasParameterizedText()
                        && !textLine.getParameterizedTextBinding().getIsOptional()) {
                    throw new PietFatalException(ErrorCode.ERR_MISSING_BINDING_VALUE,
                            String.format("Parameterized text binding %s had no content",
                                    bindingValue.getBindingId()));
                }

                setTextOnView(getBaseView(), bindingValue.getParameterizedText());
                break;
            default:
                frameContext.reportMessage(MessageType.ERROR,
                        ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT,
                        String.format("TextElement missing ParameterizedText content; has %s",
                                textLine.getContentCase()));
                setTextOnView(getBaseView(), ParameterizedText.getDefaultInstance());
        }
    }

    private void setTextOnView(TextView textView, ParameterizedText parameterizedText) {
        if (!parameterizedText.hasText()) {
            textView.setText("");
            return;
        }

        textView.setText(getTemplatedStringEvaluator().evaluate(
                getParameters().mHostProviders.getAssetProvider(), parameterizedText));
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

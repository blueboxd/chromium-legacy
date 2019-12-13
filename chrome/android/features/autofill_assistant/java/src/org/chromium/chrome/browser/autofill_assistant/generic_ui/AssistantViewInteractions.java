// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** JNI bridge between {@code interaction_handler_android} and android views. */
@JNINamespace("autofill_assistant")
public class AssistantViewInteractions {
    @CalledByNative
    public static void setOnClickListener(View view, String identifier,
            @Nullable AssistantValue value, AssistantGenericUiDelegate delegate) {
        view.setOnClickListener(unused -> { delegate.onViewClicked(identifier, value); });
    }
}

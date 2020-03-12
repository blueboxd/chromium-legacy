// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Delegate for the generic user interface. */
@JNINamespace("autofill_assistant")
public class AssistantGenericUiDelegate {
    private long mNativeAssistantGenericUiDelegate;

    @CalledByNative
    private static AssistantGenericUiDelegate create(long nativeDelegate) {
        return new AssistantGenericUiDelegate(nativeDelegate);
    }

    private AssistantGenericUiDelegate(long nativeAssistantGenericUiDelegate) {
        mNativeAssistantGenericUiDelegate = nativeAssistantGenericUiDelegate;
    }

    void onViewClicked(String identifier) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onViewClicked(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this, identifier);
    }

    void onListPopupSelectionChanged(String selectedIndicesIdentifier,
            AssistantValue selectedIndices, @Nullable String selectedNamesIdentifier,
            @Nullable AssistantValue selectedNames) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onListPopupSelectionChanged(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this,
                selectedIndicesIdentifier, selectedIndices, selectedNamesIdentifier, selectedNames);
    }

    void onCalendarPopupDateChanged(String identifier, AssistantValue value) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onCalendarPopupDateChanged(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this, identifier,
                value);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantGenericUiDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onViewClicked(long nativeAssistantGenericUiDelegate, AssistantGenericUiDelegate caller,
                String identifier);
        void onListPopupSelectionChanged(long nativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate caller, String selectedIndicesIdentifier,
                AssistantValue selectedIndices, @Nullable String selectedNamesIdentifier,
                @Nullable AssistantValue selectedNames);
        void onCalendarPopupDateChanged(long nativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate caller, String identifier, AssistantValue value);
    }
}

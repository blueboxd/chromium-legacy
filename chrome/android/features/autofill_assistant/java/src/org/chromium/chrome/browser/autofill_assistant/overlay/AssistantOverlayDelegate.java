// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Delegate for the overlay. */
@JNINamespace("autofill_assistant")
class AssistantOverlayDelegate {
    private long mNativeAssistantOverlayDelegate;

    @CalledByNative
    private static AssistantOverlayDelegate create(long nativeAssistantOverlayDelegate) {
        return new AssistantOverlayDelegate(nativeAssistantOverlayDelegate);
    }

    private AssistantOverlayDelegate(long nativeAssistantOverlayDelegate) {
        mNativeAssistantOverlayDelegate = nativeAssistantOverlayDelegate;
    }

    /** Called after a certain number of unexpected taps. */
    void onUnexpectedTaps() {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnUnexpectedTaps(mNativeAssistantOverlayDelegate);
        }
    }

    /** Asks for an update of the touchable area. */
    void updateTouchableArea() {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeUpdateTouchableArea(mNativeAssistantOverlayDelegate);
        }
    }

    /**
     * Called when interaction within allowed touchable area was detected. The interaction
     * could be any gesture.
     */
    void onUserInteractionInsideTouchableArea() {
        if (mNativeAssistantOverlayDelegate != 0) {
            nativeOnUserInteractionInsideTouchableArea(mNativeAssistantOverlayDelegate);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantOverlayDelegate = 0;
    }

    private native void nativeOnUnexpectedTaps(long nativeAssistantOverlayDelegate);
    private native void nativeUpdateTouchableArea(long nativeAssistantOverlayDelegate);
    private native void nativeOnUserInteractionInsideTouchableArea(
            long nativeAssistantOverlayDelegate);
}

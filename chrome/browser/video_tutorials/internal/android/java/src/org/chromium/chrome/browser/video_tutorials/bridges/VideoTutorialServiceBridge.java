// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.bridges;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;

import java.util.List;

/**
 * Bridge to the native video tutorial service for the given {@link Profile}.
 */
@JNINamespace("video_tutorials")
public class VideoTutorialServiceBridge implements VideoTutorialService {
    private long mNativeVideoTutorialServiceBridge;

    @CalledByNative
    private static VideoTutorialServiceBridge create(long nativePtr) {
        return new VideoTutorialServiceBridge(nativePtr);
    }

    private VideoTutorialServiceBridge(long nativePtr) {
        mNativeVideoTutorialServiceBridge = nativePtr;
    }

    @Override
    public void getTutorials(Callback<List<Tutorial>> callback) {
        callback.onResult(null);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeVideoTutorialServiceBridge = 0;
    }
}

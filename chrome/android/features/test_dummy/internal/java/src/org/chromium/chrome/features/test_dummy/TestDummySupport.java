// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.test_dummy;

import org.chromium.base.annotations.NativeMethods;

/** Support class to proxy JNI calls from module Java to module native. */
//@MainDex
public class TestDummySupport {
    /** JNI calls for exercising native parts of the DFM. */
    @NativeMethods
    public interface Natives {
        boolean openAndVerifyNativeLibrary();
    }

    public static boolean openAndVerifyNativeLibrary() {
        return TestDummySupportJni.get().openAndVerifyNativeLibrary();
    }
}

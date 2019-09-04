// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;
import org.chromium.base.annotations.NativeMethods;

abstract class GraphicsUtils {
    public static long getDrawSWFunctionTable() {
        return GraphicsUtilsJni.get().getDrawSWFunctionTable();
    }

    public static long getDrawGLFunctionTable() {
        return GraphicsUtilsJni.get().getDrawGLFunctionTable();
    }

    @NativeMethods
    interface Natives {
        long getDrawSWFunctionTable();
        long getDrawGLFunctionTable();
    }
}

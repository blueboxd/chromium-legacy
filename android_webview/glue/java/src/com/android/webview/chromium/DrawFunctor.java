// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;
import org.chromium.base.annotations.NativeMethods;

class DrawFunctor {
    public static long getDrawFnFunctionTable() {
        return DrawFunctorJni.get().getFunctionTable();
    }

    @NativeMethods
    interface Natives {
        long getFunctionTable();
    }
}

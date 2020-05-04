// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.ScriptReference;
import org.chromium.support_lib_boundary.ScriptReferenceBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between ScriptReferenceBoundaryInterface and ScriptReference.
 */
class SupportLibScriptReferenceAdapter implements ScriptReferenceBoundaryInterface {
    private ScriptReference mScriptReference;

    public SupportLibScriptReferenceAdapter(ScriptReference scriptReference) {
        mScriptReference = scriptReference;
    }

    @Override
    public void remove() {
        recordApiCall(ApiCall.REMOVE_DOCUMENT_START_SCRIPT);
        mScriptReference.remove();
    }
}

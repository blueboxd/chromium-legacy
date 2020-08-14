// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.content_public.browser.WebContents;

/**
 * Starts a native lite service and waits for script completion.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantLiteService
        implements AutofillAssistantServiceInjector.NativeServiceProvider {
    private final WebContents mWebContents;
    private final String mTriggerScriptPath;
    // Returns a state corresponding to {code LiteScriptFinishedState}.
    private Callback<Integer> mNotifyFinishedCallback;

    AutofillAssistantLiteService(WebContents webContents, String triggerScriptPath,
            Callback<Integer> notifyFinishedCallback) {
        mWebContents = webContents;
        mTriggerScriptPath = triggerScriptPath;
        mNotifyFinishedCallback = notifyFinishedCallback;
    }

    @Override
    public long createNativeService() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point the native controller will take ownership of the wrapper.
        return AutofillAssistantLiteServiceJni.get().createLiteService(
                this, mWebContents, mTriggerScriptPath);
    }

    @CalledByNative
    private void onFinished(@LiteScriptFinishedState int state) {
        if (mNotifyFinishedCallback != null) {
            mNotifyFinishedCallback.onResult(state);
            // Ignore subsequent notifications.
            mNotifyFinishedCallback = null;
        }
    }

    @NativeMethods
    interface Natives {
        long createLiteService(AutofillAssistantLiteService service, WebContents webContents,
                String triggerScriptPath);
    }
}

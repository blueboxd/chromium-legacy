// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;

/**
 * Configures and runs a lite script and returns the result to the caller.
 */
class AutofillAssistantLiteScriptCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final CompositorViewHolder mCompositorViewHolder;
    private final WebContents mWebContents;

    AutofillAssistantLiteScriptCoordinator(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            @NonNull WebContents webContents) {
        mBottomSheetController = bottomSheetController;
        mBrowserControlsStateProvider = browserControls;
        mCompositorViewHolder = compositorViewHolder;
        mWebContents = webContents;
    }

    void startLiteScript(String firstTimeUserScriptPath, String returningUserScriptPath,
            Callback<Boolean> onFinishedCallback) {
        AutofillAssistantLiteService liteService = new AutofillAssistantLiteService(mWebContents,
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser()
                        ? firstTimeUserScriptPath
                        : returningUserScriptPath,
                finishedState -> handleLiteScriptResult(finishedState, onFinishedCallback));
        AutofillAssistantServiceInjector.setServiceToInject(liteService);
        AutofillAssistantClient.fromWebContents(mWebContents)
                .start(/* initialUrl= */ "", /* parameters= */ new HashMap<>(),
                        /* experimentIds= */ "", /* callerAccount= */ "", /* userName= */ "",
                        /* isChromeCustomTab= */ true, /* onboardingCoordinator= */ null);
        AutofillAssistantServiceInjector.setServiceToInject(null);
    }

    private void handleLiteScriptResult(
            @LiteScriptFinishedState int finishedState, Callback<Boolean> onFinishedCallback) {
        // TODO(arbesser) add UKM metric for state.
        // TODO(arbesser) restart lite script on LITE_SCRIPT_BROWSE_FAILED_NAVIGATE.
        switch (finishedState) {
            case LiteScriptFinishedState.LITE_SCRIPT_UNKNOWN_FAILURE:
            case LiteScriptFinishedState.LITE_SCRIPT_SERVICE_DELETED:
            case LiteScriptFinishedState.LITE_SCRIPT_PATH_MISMATCH:
            case LiteScriptFinishedState.LITE_SCRIPT_GET_ACTIONS_FAILED:
            case LiteScriptFinishedState.LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR:
            case LiteScriptFinishedState.LITE_SCRIPT_UNSAFE_ACTIONS:
            case LiteScriptFinishedState.LITE_SCRIPT_INVALID_SCRIPT:
            case LiteScriptFinishedState.LITE_SCRIPT_BROWSE_FAILED_NAVIGATE:
            case LiteScriptFinishedState.LITE_SCRIPT_BROWSE_FAILED_OTHER:
                onFinishedCallback.onResult(false);
                return;
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_NAVIGATE:
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE:
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CLOSE:
                AutofillAssistantPreferencesUtil
                        .incrementAutofillAssistantNumberOfLiteScriptsCanceled();
                // fall through
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_OTHER:
                // The prompt was displayed on screen, hence we mark them as returning user from now
                // on.
                AutofillAssistantPreferencesUtil.setAutofillAssistantReturningLiteScriptUser();
                onFinishedCallback.onResult(false);
                return;
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED:
                onFinishedCallback.onResult(true);

                return;
        }
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Factory implementation to create AutofillAssistantClient as
 * as AutofillAssistantModuleEntry to serve as interface between
 * base module and assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryFactoryImpl
        implements AutofillAssistantModuleEntryFactory {
    @Override
    public AutofillAssistantModuleEntry createEntry(
            @NonNull ChromeActivity activity, @NonNull WebContents webContents) {
        return new AutofillAssistantModuleEntryImpl(activity, webContents);
    }

    private static class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
        private final ChromeActivity mActivity;
        private final WebContents mWebContents;

        private AutofillAssistantModuleEntryImpl(ChromeActivity activity, WebContents webContents) {
            mActivity = activity;
            mWebContents = webContents;
        }

        @Override
        public void start(boolean skipOnboarding, String initialUrl, Map<String, String> parameters,
                String experimentIds, Bundle intentExtras) {
            if (skipOnboarding) {
                AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN);

                start(initialUrl, parameters, experimentIds, intentExtras, null);
            } else {
                AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_SHOWN);

                BottomSheetController controller = mActivity.getBottomSheetController();
                AssistantBottomSheetContent content = new AssistantBottomSheetContent(mActivity);
                AssistantOverlayModel overlayModel = new AssistantOverlayModel();
                AssistantOverlayCoordinator overlayCoordinator =
                        new AssistantOverlayCoordinator(mActivity, overlayModel);
                overlayModel.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);
                AssistantOnboardingCoordinator.setOnboardingContent(
                        experimentIds, mActivity, content, accepted -> {
                            if (accepted) {
                                AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_ACCEPTED);

                                // We transfer the ownership of the overlay to #start() and the
                                // bottom sheet content will be replaced.
                                start(initialUrl, parameters, experimentIds, intentExtras,
                                        overlayCoordinator);
                            } else {
                                AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_CANCELLED);
                                overlayCoordinator.destroy();
                                controller.hideContent(content, /* animate= */ true);
                                AutofillAssistantMetrics.recordDropOut(DropOutReason.DECLINED);
                            }
                        });

                BottomSheetUtils.showContentAndExpand(controller, content);
            }
        }

        private void start(String initialUrl, Map<String, String> parameters, String experimentIds,
                Bundle intentExtras, @Nullable AssistantOverlayCoordinator overlayCoordinator) {
            AutofillAssistantClient client = AutofillAssistantClient.fromWebContents(mWebContents);
            client.start(initialUrl, parameters, experimentIds, intentExtras, overlayCoordinator);
        }
    }
}
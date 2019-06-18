// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
class AssistantCoordinator {
    interface Delegate {
        /** Completely stop the Autofill Assistant. */
        void stop(@DropOutReason int reason);

        // TODO(crbug.com/806868): Move onboarding and snackbar out of this class and remove the
        // delegate.
    }

    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    private final ChromeActivity mActivity;
    private final Delegate mDelegate;

    private final AssistantModel mModel;
    private AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    AssistantCoordinator(
            ChromeActivity activity, Delegate delegate, BottomSheetController controller) {
        mActivity = activity;
        mDelegate = delegate;
        mModel = new AssistantModel();

        // Instantiate child components.
        mBottomBarCoordinator = new AssistantBottomBarCoordinator(activity, mModel, controller);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity, mModel);
        mOverlayCoordinator = new AssistantOverlayCoordinator(activity, mModel.getOverlayModel());

        activity.getCompositorViewHolder().addCompositorViewResizer(mBottomBarCoordinator);
        mModel.setVisible(true);
    }

    /** Detaches and destroys the view. */
    public void destroy() {
        if (mActivity.getCompositorViewHolder() != null) {
            mActivity.getCompositorViewHolder().removeCompositorViewResizer(mBottomBarCoordinator);
        }

        mModel.setVisible(false);
        mOverlayCoordinator.destroy();
        mBottomBarCoordinator.destroy();
        mBottomBarCoordinator = null;
    }

    /**
     * Show the onboarding screen and call {@code onAccept} if the user agreed to proceed, shutdown
     * otherwise.
     */
    public void showOnboarding(String experimentIds, Runnable onAccept) {
        mBottomBarCoordinator.showOnboarding(experimentIds, accepted -> {
            if (accepted) {
                onAccept.run();
            } else {
                mDelegate.stop(DropOutReason.DECLINED);
            }
        });
    }

    /**
     * Get the model representing the current state of the UI.
     */

    public AssistantModel getModel() {
        return mModel;
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    /**
     * Show the Chrome feedback form.
     */
    public void showFeedback(String debugContext) {
        HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity, Profile.getLastUsedProfile(),
                mActivity.getActivityTab().getUrl(), FEEDBACK_CATEGORY_TAG,
                FeedbackContext.buildContextString(mActivity, debugContext, 4));
    }
}

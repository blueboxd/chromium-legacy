// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.LoadingView;

/**
 * Another FirstRunFragment that is only used when running with CCT.
 */
public class TosAndUmaFirstRunFragmentWithEnterpriseSupport
        extends ToSAndUMAFirstRunFragment implements LoadingView.Observer {
    private static final String TAG = "TosAndUmaFragment";

    /** FRE page that instantiates this fragment. */
    public static class Page
            implements FirstRunPage<TosAndUmaFirstRunFragmentWithEnterpriseSupport> {
        @Override
        public boolean shouldSkipPageOnCreate() {
            // TODO(crbug.com/1111490): Revisit during post-MVP.
            // There's an edge case where we accept the welcome page in the main app, abort the FRE,
            // then go through this CCT FRE again.
            return FirstRunStatus.shouldSkipWelcomePage();
        }

        @Override
        public TosAndUmaFirstRunFragmentWithEnterpriseSupport instantiateFragment() {
            return new TosAndUmaFirstRunFragmentWithEnterpriseSupport();
        }
    }

    private boolean mViewCreated;
    private LoadingView mLoadingSpinner;
    private CallbackController mCallbackController;

    /**
     * Whether app restriction is found on the device. This can be null when this information is not
     * ready yet.
     */
    private @Nullable Boolean mHasRestriction;
    /**
     * The value of CCTToSDialogEnabled policy on the device. If the value is false, it means we
     * should skip the rest of FRE. This can be null when this information is not ready yet.
     */
    private @Nullable Boolean mPolicyCctTosDialogEnabled;

    private static boolean sBlockPolicyLoadingForTest;

    private TosAndUmaFirstRunFragmentWithEnterpriseSupport() {
        mCallbackController = new CallbackController();
        checkAppRestriction();
    }

    @Override
    public void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mLoadingSpinner != null) {
            mLoadingSpinner.destroy();
            mLoadingSpinner = null;
        }
        super.onDestroy();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mLoadingSpinner = view.findViewById(R.id.progress_spinner_large);
        mViewCreated = true;

        if (shouldWaitForPolicyLoading()) {
            mLoadingSpinner.addObserver(this);
            mLoadingSpinner.showLoadingUI();
            setTosAndUmaVisible(false);
        } else if (confirmedCctTosDialogDisabled()) {
            // Skip the FRE if we know dialog is disabled by policy.
            exitCctFirstRun();
        }
    }

    @Override
    public void onNativeInitialized() {
        super.onNativeInitialized();

        if (shouldWaitForPolicyLoading()) {
            checkEnterprisePolicies();
        }
    }

    @Override
    protected boolean canShowUmaCheckBox() {
        return super.canShowUmaCheckBox() && confirmedToShowUmaAndTos();
    }

    @Override
    public void onHideLoadingUIComplete() {
        if (confirmedCctTosDialogDisabled()) {
            // TODO(crbug.com/1108564): Show the different UI that has the enterprise disclosure.
            exitCctFirstRun();
        } else {
            // Else, show the UMA as the loading spinner is GONE.
            assert confirmedToShowUmaAndTos();
            setTosAndUmaVisible(true);
        }
    }

    /**
     * @return True if we need to wait on an Async tasks that determine whether any Enterprise
     *         policies needs to be applied. If this returns false, then we no longer need to wait
     *         and can update the UI immediately.
     */
    private boolean shouldWaitForPolicyLoading() {
        return !confirmedNoAppRestriction() && mPolicyCctTosDialogEnabled == null;
    }

    /**
     * This methods will return true only when we know either 1) there's no on-device app
     * restrictions or 2) policies has been loaded and first run has not been disabled via policy.
     *
     * @return Whether we should show TosAndUma components on the UI.
     */
    private boolean confirmedToShowUmaAndTos() {
        return confirmedNoAppRestriction() || confirmedCctTosDialogEnabled();
    }

    private boolean confirmedNoAppRestriction() {
        return mHasRestriction != null && !mHasRestriction;
    }

    private boolean confirmedCctTosDialogEnabled() {
        return mPolicyCctTosDialogEnabled != null && mPolicyCctTosDialogEnabled;
    }

    private boolean confirmedCctTosDialogDisabled() {
        return mPolicyCctTosDialogEnabled != null && !mPolicyCctTosDialogEnabled;
    }

    private void checkAppRestriction() {
        FirstRunAppRestrictionInfo.getInstance().getHasAppRestriction(
                mCallbackController.makeCancelable(this::onAppRestrictionDetected));
    }

    @VisibleForTesting
    void onAppRestrictionDetected(boolean hasAppRestriction) {
        mHasRestriction = hasAppRestriction;

        if (!shouldWaitForPolicyLoading() && mViewCreated) {
            // TODO(crbug.com/1106812): Unregister policy listener.
            mLoadingSpinner.hideLoadingUI();
        }
    }

    private void checkEnterprisePolicies() {
        // TODO(crbug.com/1106812): Monitor policy changes when it is ready.
        if (!sBlockPolicyLoadingForTest) {
            onCctTosPolicyDetected(getPolicyCctTosDialogEnabled());
        }
    }

    private boolean getPolicyCctTosDialogEnabled() {
        // TODO(crbug.com/1108118): Do the actual fetching for CCT Policy to replace the fake one.
        return true;
    }

    @VisibleForTesting
    void onCctTosPolicyDetected(boolean cctTosDialogEnabled) {
        mPolicyCctTosDialogEnabled = cctTosDialogEnabled;
        if (mViewCreated) {
            mLoadingSpinner.hideLoadingUI();
        }
    }

    @VisibleForTesting
    void exitCctFirstRun() {
        assert confirmedCctTosDialogDisabled();
        // TODO(crbug.com/1108564): Fire a signal to end this fragment when disclaimer is ready.
        // TODO(crbug.com/1108582): Save a shared pref indicating Enterprise CCT FRE is complete,
        //  and skip waiting for future cold starts.
        Log.d(TAG, "TosAndUmaFirstRunFragmentWithEnterpriseSupport finished.");
        getPageDelegate().exitFirstRun();
    }

    @VisibleForTesting
    static void setBlockPolicyLoadingForTest(boolean blockPolicyLoadingForTest) {
        sBlockPolicyLoadingForTest = blockPolicyLoadingForTest;
    }
}

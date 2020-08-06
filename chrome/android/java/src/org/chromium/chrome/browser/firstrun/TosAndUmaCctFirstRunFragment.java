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

/**
 * Another FirstRunFragment that is only used when running with CCT.
 */
public class TosAndUmaCctFirstRunFragment extends ToSAndUMAFirstRunFragment {
    private static final String TAG = "TosAndUmaCctFre";

    /** FRE page that instantiates this fragment. */
    public static class Page implements FirstRunPage<TosAndUmaCctFirstRunFragment> {
        @Override
        public boolean shouldSkipPageOnCreate() {
            // TODO(crbug.com/1111490): Revisit during post-MVP.
            // There's an edge case where we accept the welcome page in the main app, abort the FRE,
            // then go through this CCT FRE again.
            return FirstRunStatus.shouldSkipWelcomePage();
        }

        @Override
        public TosAndUmaCctFirstRunFragment instantiateFragment() {
            return new TosAndUmaCctFirstRunFragment();
        }
    }

    private boolean mViewCreated;
    private View mLargeSpinner;
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

    private TosAndUmaCctFirstRunFragment() {
        mCallbackController = new CallbackController();
        checkAppRestriction();
    }

    @Override
    public void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        super.onDestroy();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mLargeSpinner = view.findViewById(R.id.progress_spinner_large);
        mViewCreated = true;

        if (shouldWaitForPolicyLoading()) {
            // TODO(crbug.com/1106987): Post a task to show the LargeSpinner after 500ms and make
            // sure it appears 500 ms at least.
            mLargeSpinner.setVisibility(View.VISIBLE);
            setTosAndUmaVisible(false);
        } else {
            updateViewOnEnterpriseChecksComplete();
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
        return super.canShowUmaCheckBox() && shouldShowUmaAndTos();
    }

    /**
     * @return True if we need to wait on an Async tasks that determine whether any Enterprise
     *         policies needs to be applied. If this returns false, then we no longer need to wait
     *         and can update the UI immediately.
     */
    private boolean shouldWaitForPolicyLoading() {
        return (mHasRestriction == null || mHasRestriction) && mPolicyCctTosDialogEnabled == null;
    }

    private boolean shouldShowUmaAndTos() {
        return (mHasRestriction != null && !mHasRestriction)
                || (mPolicyCctTosDialogEnabled != null && mPolicyCctTosDialogEnabled);
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
            updateViewOnEnterpriseChecksComplete();
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
        updateViewOnEnterpriseChecksComplete();
    }

    /**
     * Update the UI based on aggregated signal whether ToS / UMA should be shown.
     */
    private void updateViewOnEnterpriseChecksComplete() {
        if (shouldShowUmaAndTos()) {
            mLargeSpinner.setVisibility(View.GONE);
            setTosAndUmaVisible(true);
            return;
        }

        assert mPolicyCctTosDialogEnabled != null && !mPolicyCctTosDialogEnabled;

        // TODO(crbug.com/1108564): Show the different UI that has the enterprise disclosure.
        // TODO(crbug.com/1106987): Post a task to show the LargeSpinner after 500ms and make sure
        // it appears 500 ms at least.
        mLargeSpinner.setVisibility(View.GONE);
        exitCctFirstRun();
    }

    @VisibleForTesting
    void exitCctFirstRun() {
        // TODO(crbug.com/1108564): Fire a signal to end this fragment when disclaimer is ready.
        // TODO(crbug.com/1108582): Save a shared pref indicating Enterprise CCT FRE is complete,
        //  and skip waiting for future cold starts.
        Log.d(TAG, "ToSAndUMACCTFirstRunFragment finished.");
        getPageDelegate().exitFirstRun();
    }

    @VisibleForTesting
    static void setBlockPolicyLoadingForTest(boolean blockPolicyLoadingForTest) {
        sBlockPolicyLoadingForTest = blockPolicyLoadingForTest;
    }
}

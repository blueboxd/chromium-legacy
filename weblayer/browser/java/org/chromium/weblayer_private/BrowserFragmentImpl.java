// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.view.View;

import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.weblayer_private.aidl.BrowserFragmentArgs;
import org.chromium.weblayer_private.aidl.IBrowserFragment;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IRemoteFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;

public class BrowserFragmentImpl extends RemoteFragmentImpl {

    private final ProfileImpl mProfile;

    private BrowserFragmentControllerImpl mController;
    private Context mContext;

    // TODO(cduvall): Factor out the logic we need from ActivityWindowAndroid so we do not inherit
    // directly from it.
    private static class FragmentWindowAndroid extends ActivityWindowAndroid {
        private BrowserFragmentImpl mFragment;

        FragmentWindowAndroid(Context context, BrowserFragmentImpl fragment) {
            // Use false to disable listening to activity state.
            super(context, false);
            mFragment = fragment;
        }

        @Override
        protected boolean startIntentSenderForResult(IntentSender intentSender, int requestCode) {
            return mFragment.startIntentSenderForResult(
                    intentSender, requestCode, new Intent(), 0, 0, 0, null);
        }

        @Override
        protected boolean startActivityForResult(Intent intent, int requestCode) {
            return mFragment.startActivityForResult(intent, requestCode, null);
        }
    }

    public BrowserFragmentImpl(ProfileManager profileManager, IRemoteFragmentClient client,
            Bundle fragmentArgs) {
        super(client);
        mProfile = profileManager.getProfile(fragmentArgs.getString(
                BrowserFragmentArgs.PROFILE_PATH));
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
        if (mController != null) { // On first creation, onAttach is called before onCreate
            mController.onFragmentAttached(context, new FragmentWindowAndroid(context, this));
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mController = new BrowserFragmentControllerImpl(mProfile, savedInstanceState);
        if (mContext != null) {
            mController.onFragmentAttached(mContext, new FragmentWindowAndroid(mContext, this));
        }
    }

    @Override
    public View onCreateView() {
        return mController.getFragmentView();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mController.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mController.destroy();
        mController = null;
    }

    @Override
    public void onDetach() {
        super.onDetach();
        // mController != null if fragment is retained, otherwise onDestroy is called first.
        if (mController != null) {
            mController.onFragmentDetached();
        }
        mContext = null;
    }

    public IBrowserFragment asIBrowserFragment() {
        return new IBrowserFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                return BrowserFragmentImpl.this;
            }

            @Override
            public IBrowserFragmentController getController() {
                if (mController == null) {
                    throw new RuntimeException("BrowserFragmentController is available only between"
                            + " BrowserFragment's onCreate() and onDestroy().");
                }
                return mController;
            }
        };
    }
}

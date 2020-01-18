// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of RemoteFragmentImpl which forwards logic to BrowserImpl.
 */
public class BrowserFragmentImpl extends RemoteFragmentImpl {
    private final ProfileImpl mProfile;

    private BrowserImpl mBrowser;
    private Context mContext;

    public BrowserFragmentImpl(
            ProfileManager profileManager, IRemoteFragmentClient client, Bundle fragmentArgs) {
        super(client);
        mProfile =
                profileManager.getProfile(fragmentArgs.getString(BrowserFragmentArgs.PROFILE_NAME));
    }

    @Override
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);
        mContext = ClassLoaderContextWrapperFactory.get(context);
        if (mBrowser != null) { // On first creation, onAttach is called before onCreate
            mBrowser.onFragmentAttached(mContext, new FragmentWindowAndroid(mContext, this));
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        super.onCreate(savedInstanceState);
        mBrowser = new BrowserImpl(mProfile, savedInstanceState);
        if (mContext != null) {
            mBrowser.onFragmentAttached(mContext, new FragmentWindowAndroid(mContext, this));
        }
    }

    @Override
    public View onCreateView() {
        StrictModeWorkaround.apply();
        return mBrowser.getFragmentView();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        StrictModeWorkaround.apply();
        mBrowser.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        StrictModeWorkaround.apply();
        mBrowser.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mBrowser.destroy();
        mBrowser = null;
    }

    @Override
    public void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        // mBrowser != null if fragment is retained, otherwise onDestroy is called first.
        if (mBrowser != null) {
            mBrowser.onFragmentDetached();
        }
        mContext = null;
    }

    public IBrowserFragment asIBrowserFragment() {
        return new IBrowserFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return BrowserFragmentImpl.this;
            }

            @Override
            public IBrowser getBrowser() {
                StrictModeWorkaround.apply();
                if (mBrowser == null) {
                    throw new RuntimeException("Browser is available only between"
                            + " BrowserFragment's onCreate() and onDestroy().");
                }
                return mBrowser;
            }
        };
    }
}

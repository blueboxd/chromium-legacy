// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IRemoteFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * WebLayer's fragment implementation.
 */
public final class BrowserFragment extends Fragment {
    private final IRemoteFragmentClient mClientImpl = new IRemoteFragmentClient.Stub() {
        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            BrowserFragment.super.onAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnStart() {
            BrowserFragment.super.onStart();
        }

        @Override
        public void superOnResume() {
            BrowserFragment.super.onResume();
        }

        @Override
        public void superOnPause() {
            BrowserFragment.super.onPause();
        }

        @Override
        public void superOnStop() {
            BrowserFragment.super.onStop();
        }

        @Override
        public void superOnDestroyView() {
            BrowserFragment.super.onDestroyView();
        }

        @Override
        public void superOnDetach() {
            BrowserFragment.super.onDetach();
        }

        @Override
        public void superOnDestroy() {
            BrowserFragment.super.onDestroy();
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            BrowserFragment.super.onSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            return ObjectWrapper.wrap(BrowserFragment.this.getActivity());
        }
    };

    private IRemoteFragment mRemoteFragment;

    // TODO(pshmakov): how do we deal with FragmentManager restoring this Fragment on its own?
    /* package */ void setRemoteFragment(IRemoteFragment remoteFragment) {
        mRemoteFragment = remoteFragment;
    }

    /* package */ IRemoteFragmentClient asIRemoteFragmentClient() {
        return mClientImpl;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        try {
            return ObjectWrapper.unwrap(mRemoteFragment.handleOnCreateView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onAttach(Context context) {
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        try {
            mRemoteFragment.handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        try {
            mRemoteFragment.handleOnActivityCreated(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStart() {
        try {
            mRemoteFragment.handleOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onResume() {
        try {
            mRemoteFragment.handleOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onSaveInstanceState(Bundle outState) {
        try {
            mRemoteFragment.handleOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onPause() {
        try {
            mRemoteFragment.handleOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStop() {
        try {
            mRemoteFragment.handleOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroyView() {
        try {
            mRemoteFragment.handleOnDestroyView();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroy() {
        try {
            mRemoteFragment.handleOnDestroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDetach() {
        try {
            mRemoteFragment.handleOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}

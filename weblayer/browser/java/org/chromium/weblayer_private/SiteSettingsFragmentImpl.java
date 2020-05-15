// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;

import org.chromium.components.browser_ui.styles.R;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ISiteSettingsFragment;
import org.chromium.weblayer_private.interfaces.SiteSettingsFragmentArgs;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * WebLayer's implementation of the client library's SiteSettingsFragment.
 *
 * This class creates an instance of the Fragment given in its FRAGMENT_NAME argument, and forwards
 * all incoming lifecycle events from SiteSettingsFragment to it. Because Fragments created in
 * WebLayer use a different AndroidX library than the embedder's, we can't attach the Fragment
 * created here directly to the embedder's Fragment tree, and have to create a local
 * FragmentController to manage it.
 */
public class SiteSettingsFragmentImpl extends RemoteFragmentImpl {
    private final ProfileImpl mProfile;
    private final String mFragmentName;
    private final Bundle mFragmentArguments;

    // The WebLayer-wrapped context object. This context gets assets and resources from WebLayer,
    // not from the embedder. Use this for the most part, especially to resolve WebLayer-specific
    // resource IDs.
    private Context mContext;

    public SiteSettingsFragmentImpl(ProfileManager profileManager,
            IRemoteFragmentClient remoteFragmentClient, Bundle fragmentArgs) {
        super(remoteFragmentClient);
        mProfile = profileManager.getProfile(
                fragmentArgs.getString(SiteSettingsFragmentArgs.PROFILE_NAME));
        mFragmentName = fragmentArgs.getString(SiteSettingsFragmentArgs.FRAGMENT_NAME);
        mFragmentArguments = fragmentArgs.getBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS);
    }

    @Override
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);

        mContext = new ContextThemeWrapper(
                ClassLoaderContextWrapperFactory.get(context), R.style.Theme_BrowserUI);
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mContext = null;
    }

    // TODO: Actually implement this.

    public ISiteSettingsFragment asISiteSettingsFragment() {
        return new ISiteSettingsFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return SiteSettingsFragmentImpl.this;
            }
        };
    }
}

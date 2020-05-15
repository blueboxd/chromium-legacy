// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * The client-side implementation of a SiteSettingsFragment.
 *
 * This is a Fragment that can be shown within an embedder's UI, and proxies its lifecycle events to
 * a SiteSettingsFragmentImpl object on the implementation side.
 */
public class SiteSettingsFragment extends RemoteFragment {
    public SiteSettingsFragment() {
        super();
    }

    @Override
    protected IRemoteFragment createRemoteFragment(Context appContext) {
        try {
            Bundle args = getArguments();
            if (args == null) {
                throw new RuntimeException("SiteSettingsFragment was created without arguments.");
            }
            return WebLayer.loadSync(appContext)
                    .connectSiteSettingsFragment(getRemoteFragmentClient(), args)
                    .asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }
}

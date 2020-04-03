// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.IBinder;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.IWebLayerFactory;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Factory used to create WebLayer as well as verify compatibility.
 * This is constructed by the client library using reflection.
 */
@UsedByReflection("WebLayer")
public final class WebLayerFactoryImpl extends IWebLayerFactory.Stub {
    private static int sClientMajorVersion;
    private static String sClientVersion;

    /**
     * This function is called by the client using reflection.
     *
     * @param clientVersion The full version string the client was compiled from.
     * @param clientMajorVersion The major version number the client was compiled from. This is also
     *         contained in clientVersion.
     * @param clientWebLayerVersion The version from interfaces.WebLayerVersion the client was
     *         compiled with.
     */
    @UsedByReflection("WebLayer")
    public static IBinder create(
            String clientVersion, int clientMajorVersion, int clientWebLayerVersion) {
        return new WebLayerFactoryImpl(clientVersion, clientMajorVersion);
    }

    private WebLayerFactoryImpl(String clientVersion, int clientMajorVersion) {
        sClientMajorVersion = clientMajorVersion;
        sClientVersion = clientVersion;
    }

    /**
     * Returns true if the client compiled with the specific version is compatible with this
     * implementation. The client library calls this exactly once.
     */
    @Override
    public boolean isClientSupported() {
        StrictModeWorkaround.apply();
        return Math.abs(sClientMajorVersion - getImplementationMajorVersion()) <= 4;
    }

    /**
     * Returns the major version of the implementation.
     */
    @Override
    public int getImplementationMajorVersion() {
        StrictModeWorkaround.apply();
        return VersionConstants.PRODUCT_MAJOR_VERSION;
    }

    static int getClientMajorVersion() {
        if (sClientMajorVersion == 0) {
            throw new IllegalStateException(
                    "This should only be called once WebLayer is initialized");
        }
        return sClientMajorVersion;
    }

    /**
     * Returns the full version string of the implementation.
     */
    @Override
    public String getImplementationVersion() {
        StrictModeWorkaround.apply();
        return VersionConstants.PRODUCT_VERSION;
    }

    @Override
    public IWebLayer createWebLayer() {
        StrictModeWorkaround.apply();
        assert isClientSupported();
        return new WebLayerImpl();
    }
}

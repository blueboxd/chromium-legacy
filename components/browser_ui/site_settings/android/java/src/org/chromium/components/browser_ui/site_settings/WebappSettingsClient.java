// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/**
 * An interface implemented by the embedder that allows the Site Settings UI to access
 * Webapp (TWA/PWA) related embedder-specific logic.
 */
public interface WebappSettingsClient {
    /**
     * @return The set of all origins that have a WebAPK or TWA installed.
     */
    Set<String> getOriginsWithInstalledApp();

    /**
     * @return The set of all origins whose notification permissions are delegated to another app.
     */
    Set<String> getAllDelegatedNotificationOrigins();

    /**
     * @return The user visible name of the app that will handle notification permission delegation
     *         for the origin.
     */
    @Nullable
    String getNotificationDelegateAppNameForOrigin(Origin origin);

    /**
     * @return The package name of the app that should handle notification permission delegation
     *         for the origin.
     */
    @Nullable
    String getNotificationDelegatePackageNameForOrigin(Origin origin);
}

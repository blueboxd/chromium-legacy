// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/**
 * An interface implemented by the embedder that allows the Site Settings UI to access
 * notification related embedder-specific logic.
 */
public interface NotificationSettingsClient {
    /**
     * @return The set of all origins whose notification permissions are delegated to another app.
     */
    Set<String> getAllDelegatedOrigins();

    /**
     * @return The user visible name of the app that will handle permission delegation for the
     *         origin.
     */
    @Nullable
    String getDelegateAppNameForOrigin(Origin origin);

    /**
     * @return The package name of the app that should handle permission delegation for the origin.
     */
    @Nullable
    String getDelegatePackageNameForOrigin(Origin origin);

    /**
     * @return The id of the notification channel associated with the given origin.
     */
    // TODO(crbug.com/1069895): Remove this once WebLayer supports notifications.
    String getChannelIdForOrigin(String origin);
}

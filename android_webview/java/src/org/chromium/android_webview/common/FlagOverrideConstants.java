// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Constants to facilitate communication with {@code FlagOverrideContentProvider}.
 */
public final class FlagOverrideConstants {
    // Do not instantiate this class.
    private FlagOverrideConstants() {}

    public static final String URI_AUTHORITY_SUFFIX = ".FlagOverrideContentProvider";
    public static final String URI_PATH = "/flag-overrides";
    public static final String FLAG_NAME_COLUMN = "flagName";
    public static final String FLAG_STATE_COLUMN = "flagState";
}

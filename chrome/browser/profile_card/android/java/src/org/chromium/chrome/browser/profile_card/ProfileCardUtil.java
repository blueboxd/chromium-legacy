// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.ArrayList;

/** Provide util functions for profile card component. */
public class ProfileCardUtil {
    ProfileCardUtil() {}

    /** Determines whether to show the profile card entry point. */
    public static boolean shouldShowEntryPoint() {
        // TODO(crbug/1053611): add logic about whether to show the profile card entry point.
        return false;
    }

    /** Talks to the backend and gets the creator's meta data. */
    public static void getCreatorMetaData(
            Callback<CreatorMetadata> callback, NavigationHandle navigationHandle) {
        // TODO(crbug/1053610): queries data from backend and transfers the result to
        // CreatorMetadata
    }

    /** Talks to the backend and gets the a list of ContentPreviewPostData. */
    public static ArrayList<ContentPreviewPostData> getContentPreviewPostDataList(String url) {
        // TODO(crbug/1057209): queries data from backend and transfers the result to
        // ContentPreviewPostData list.
        return null;
    }
}

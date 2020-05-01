// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to the snippets to display on the NTP using the C++ ContentSuggestionsService.
 */
public class SnippetsBridge {
    /**
     * Notifies that Chrome on Android has been upgraded.
     */
    public static void onBrowserUpgraded() {
        SnippetsBridgeJni.get().remoteSuggestionsSchedulerOnBrowserUpgraded();
    }

    /**
     * Notifies that the persistent fetching scheduler woke up.
     */
    public static void onPersistentSchedulerWakeUp() {
        SnippetsBridgeJni.get().remoteSuggestionsSchedulerOnPersistentSchedulerWakeUp();
    }

    // TODO(https://crbug.com/1069183): finish cleaning up with Feed C++ clean-up
    @NativeMethods
    interface Natives {
        long init(SnippetsBridge caller, Profile profile);
        void destroy(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        void reloadSuggestions(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        void remoteSuggestionsSchedulerOnPersistentSchedulerWakeUp();
        void remoteSuggestionsSchedulerOnBrowserUpgraded();
        boolean areRemoteSuggestionsEnabled(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        int[] getCategories(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        int getCategoryStatus(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        void fetchSuggestionImage(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category,
                String idWithinCategory, Callback<Bitmap> callback);
        void fetchSuggestionFavicon(long nativeNTPSnippetsBridge, SnippetsBridge caller,
                int category, String idWithinCategory, int minimumSizePx, int desiredSizePx,
                Callback<Bitmap> callback);
        void dismissSuggestion(long nativeNTPSnippetsBridge, SnippetsBridge caller, String url,
                int globalPosition, int category, int positionInCategory, String idWithinCategory);
        void dismissCategory(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        void restoreDismissedCategories(long nativeNTPSnippetsBridge, SnippetsBridge caller);
    }
}

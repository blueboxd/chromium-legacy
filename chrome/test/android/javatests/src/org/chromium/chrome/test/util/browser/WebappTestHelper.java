// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.content.Intent;

import org.chromium.chrome.browser.ShortcutHelper;

/** Helper class for webapp tests. */
public class WebappTestHelper {
    /**
     * Returns simplest intent which builds valid WebappInfo via {@link WebappInfo#create()}.
     */
    public static Intent createMinimalWebappIntent(String id, String url) {
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, id);
        intent.putExtra(ShortcutHelper.EXTRA_URL, url);
        return intent;
    }
}

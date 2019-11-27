// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore.internal;

import org.chromium.chrome.browser.feed.library.api.internal.store.Store;

public interface ClearableStore extends Store {
    /** Clear all data stored in the store */
    boolean clearAll();
}

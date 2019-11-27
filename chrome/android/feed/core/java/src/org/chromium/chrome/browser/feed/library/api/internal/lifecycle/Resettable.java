// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.lifecycle;

/** Interface for objects which support reset to an initial state. */
public interface Resettable {
    /* Reset the object. */
    void reset();
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.logging;

/** Interface for objects that know how to dump details of their state to a {@link Dumper}. */
public interface Dumpable {
    /** Called to dump an object into a {@link Dumper}. */
    void dump(Dumper dumper);
}

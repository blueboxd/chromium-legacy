// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

/** A structural mutation against a session. */
public interface SessionMutation {
    /** Add or update a structure in the store. */
    SessionMutation add(StreamStructure streamStructure);

    /** Commit the mutations to the backing store */
    Boolean commit();
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.store;

import com.google.search.now.feed.client.StreamDataProto.StreamStructure;

/** A structural mutation against a session. */
public interface SessionMutation {
    /** Add or update a structure in the store. */
    SessionMutation add(StreamStructure streamStructure);

    /** Commit the mutations to the backing store */
    Boolean commit();
}

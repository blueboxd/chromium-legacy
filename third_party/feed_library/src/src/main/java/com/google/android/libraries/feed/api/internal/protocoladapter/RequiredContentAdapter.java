// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.protocoladapter;

import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DataOperationProto.DataOperation;

import java.util.List;

/**
 * Interface that creates an extension point where implementers can indicate that a DataOperation is
 * dependent on other content.
 */
public interface RequiredContentAdapter {
    List<ContentId> determineRequiredContentIds(DataOperation dataOperation);
}

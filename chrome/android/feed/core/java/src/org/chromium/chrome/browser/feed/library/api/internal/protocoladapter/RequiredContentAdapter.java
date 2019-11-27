// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.protocoladapter;

import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.DataOperationProto.DataOperation;

import java.util.List;

/**
 * Interface that creates an extension point where implementers can indicate that a DataOperation is
 * dependent on other content.
 */
public interface RequiredContentAdapter {
    List<ContentId> determineRequiredContentIds(DataOperation dataOperation);
}

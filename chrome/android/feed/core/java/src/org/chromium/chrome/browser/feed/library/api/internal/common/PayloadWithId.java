// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;

/**
 * Structure style class which binds a String contentId with a {@link StreamPayload}. The class is
 * immutable and provides access to the fields directly.
 */
public final class PayloadWithId {
    public final String contentId;
    public final StreamPayload payload;

    public PayloadWithId(String contentId, StreamPayload payload) {
        this.contentId = contentId;
        this.payload = payload;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.List;

/** Allows Piet to report events to the host. */
public interface EventLogger {
    /**
     * Tells the host each {@link ErrorCode} that was raised during the binding of a Frame. This
     * list can contain duplicates.
     */
    void logEvents(List<ErrorCode> errorCodes);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** PaymentHandler UI properties, which fully describe the state of the UI. */
/* package */ class PaymentHandlerProperties {
    /** The visible height of the PaymentHandler UI's content area in pixels. */
    /* package */ static final WritableIntPropertyKey CONTENT_VISIBLE_HEIGHT_PX =
            new WritableIntPropertyKey();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CONTENT_VISIBLE_HEIGHT_PX};

    // Prevent instantiation.
    private PaymentHandlerProperties() {}
}

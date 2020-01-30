// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cardrenderer;

import android.view.View;

/**
 * Creates, owns, and manages an exposed view. Can be rebound to a given implementation specific
 * payload.
 */
public interface CardRendererAdapter {
    /**
     * Rebinds the associated view to the given payload.
     * @param protoPayload The payload that should describe what the view should do.
     */
    void bind(byte[] protoPayload);

    /**
     * @return The single associated view.
     */
    View getView();
}

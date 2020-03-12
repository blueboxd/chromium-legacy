// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.view.View;

/**
 * The top level coordinator for the query tiles UI.
 */
public interface QueryTileCoordinator {
    /** @return A {@link View} representing this coordinator. */
    View getView();
}

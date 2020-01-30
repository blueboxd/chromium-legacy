// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cardrenderer;

import android.content.Context;

/**
 * Creates CardRendererAdapters on demand.
 */
public interface CardRendererAdapterFactory {
    /**
     * @param context The context that any new Android UI objects should be created within.
     * @return A new wrapper capable of making view objects.
     */
    CardRendererAdapter createCardRendererAdapter(Context context);
}

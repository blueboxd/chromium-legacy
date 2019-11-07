// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used for handling new tabs (such as occurs when window.open() is called). If this is not
 * set, popups are disabled.
 */
public abstract class NewTabCallback {
    public abstract void onNewTab(@NonNull Tab tab, @NewTabType int type);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.internal.actionmanager;

import com.google.android.libraries.feed.api.internal.common.DismissActionWithSemanticProperties;
import com.google.android.libraries.feed.common.Result;

import java.util.List;

/** Interface for reading various {@link StreamAction}s. */
public interface ActionReader {
    /**
     * Retrieves list of {@link DismissActionWithSemanticProperties} for all valid dismiss actions
     */
    Result<List<DismissActionWithSemanticProperties>> getDismissActionsWithSemanticProperties();
}

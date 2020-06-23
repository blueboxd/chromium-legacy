// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

/**
 * Interface for storing information related to each Safety check element
 * (passwords, updates, etc) in the model.
 */
public interface SafetyCheckElement {
    /**
     * @return The resource ID for the corresponding status string.
     */
    public int getStatusString();
}

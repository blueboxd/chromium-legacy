// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import org.chromium.chrome.browser.widget.prefeditor.EditableOption;

/**
 * Represents a single login choice.
 *
 * <p>Note: currently, login choices are always considered 'complete'.</p>
 */
public class AssistantLoginChoice extends EditableOption {
    private final int mPriority;
    /**
     * @param identifier The unique identifier of this login choice.
     * @param label The label to display to the user.
     * @param priority The priority of this login choice (lower value == higher priority). Can be -1
     * to indicate default/auto.
     */
    public AssistantLoginChoice(String identifier, String label, int priority) {
        super(identifier, label, null, null);
        mPriority = priority;
    }

    public int getPriority() {
        return mPriority;
    }
}

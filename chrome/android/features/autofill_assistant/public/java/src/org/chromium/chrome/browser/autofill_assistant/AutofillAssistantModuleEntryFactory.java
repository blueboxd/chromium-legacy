// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.support.annotation.NonNull;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.components.module_installer.ModuleInterface;
import org.chromium.content_public.browser.WebContents;

/**
 * Interface to create AutofillAssistantModuleEntry as inferface
 * between base module and assistant DFM.
 */
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.chrome.browser.autofill_assistant."
                + "AutofillAssistantModuleEntryFactoryImpl")
interface AutofillAssistantModuleEntryFactory {
    AutofillAssistantModuleEntry createEntry(
            @NonNull ChromeActivity activity, @NonNull WebContents webContents);
}
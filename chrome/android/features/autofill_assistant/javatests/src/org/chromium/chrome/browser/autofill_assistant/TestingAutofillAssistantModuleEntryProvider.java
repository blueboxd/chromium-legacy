// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implementation of {@link AutofillAssistantModuleEntryProvider} that can be manipulated to
 * simulate missing or uninstallable module.
 */
class TestingAutofillAssistantModuleEntryProvider extends AutofillAssistantModuleEntryProvider {
    private boolean mNotInstalled;
    private boolean mCannotInstall;

    /** The module is already installed. This is the default state. */
    public void setInstalled() {
        mNotInstalled = false;
        mCannotInstall = false;
    }

    /** The module is not installed, but can be installed. */
    public void setNotInstalled() {
        mNotInstalled = true;
        mCannotInstall = false;
    }

    /** The module is not installed, and cannot be installed. */
    public void setCannotInstall() {
        mNotInstalled = true;
        mCannotInstall = true;
    }

    @Override
    public AutofillAssistantModuleEntry getModuleEntryIfInstalled() {
        if (mNotInstalled) return null;
        return super.getModuleEntryIfInstalled();
    }

    @Override
    public void getModuleEntry(Tab tab, Callback<AutofillAssistantModuleEntry> callback) {
        if (mCannotInstall) {
            callback.onResult(null);
            return;
        }
        mNotInstalled = false;
        super.getModuleEntry(tab, callback);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import org.junit.rules.ExternalResource;

/**
 * This rule allows the caller to specify their own {@link ModuleInstaller} for the duration of the
 * test and resets it back to what it was before.
 *
 * TODO(wnwen): This should eventually become ModuleConfigRule.
 */
public class ModuleInstallerRule extends ExternalResource {
    private ModuleInstaller mOldModuleInstaller;
    private final ModuleInstaller mMockModuleInstaller;

    public ModuleInstallerRule(ModuleInstaller mockModuleInstaller) {
        mMockModuleInstaller = mockModuleInstaller;
    }

    @Override
    protected void before() {
        mOldModuleInstaller = ModuleInstaller.getInstance();
        ModuleInstaller.setInstanceForTesting(mMockModuleInstaller);
    }

    @Override
    protected void after() {
        ModuleInstaller.setInstanceForTesting(mOldModuleInstaller);
    }
}

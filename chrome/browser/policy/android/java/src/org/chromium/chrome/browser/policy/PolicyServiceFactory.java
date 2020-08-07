// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.policy.PolicyService;

/**
 * Get the PolicyService instance. Note that the associated C++ instance won't
 * notify its deletion. It's caller's responsibility to make sure the instance
 * is still valid.
 */
@JNINamespace("policy::android")
public class PolicyServiceFactory {
    /**
     * Returns the PolicyService instance that contains browser policies.
     * The associated C++ instance is deleted during shutdown.
     */
    public static PolicyService getGlobalPolicyService() {
        return PolicyServiceFactoryJni.get().getGlobalPolicyService();
    }

    /**
     * Returns the PolicyService instance that contains |profile|'s policies.
     * The associated C++ instance is deleted during shutdown or {@link Profile}
     * deletion.
     */
    public static PolicyService getProfilePolicyService(Profile profile) {
        return PolicyServiceFactoryJni.get().getProfilePolicyService(profile);
    }

    @NativeMethods
    public interface Natives {
        PolicyService getGlobalPolicyService();
        PolicyService getProfilePolicyService(Profile profile);
    }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Wrapper for native dom_distiller::DomDistillerService.
 */
@JNINamespace("dom_distiller::android")
public final class DomDistillerService {

    private final long mDomDistillerServiceAndroid;
    private final DistilledPagePrefs mDistilledPagePrefs;

    private DomDistillerService(long nativeDomDistillerAndroidServicePtr) {
        mDomDistillerServiceAndroid = nativeDomDistillerAndroidServicePtr;
        mDistilledPagePrefs = new DistilledPagePrefs(
                DomDistillerServiceJni.get().getDistilledPagePrefsPtr(mDomDistillerServiceAndroid));
    }

    public DistilledPagePrefs getDistilledPagePrefs() {
        return mDistilledPagePrefs;
    }

    public boolean hasEntry(String entryId) {
        return DomDistillerServiceJni.get().hasEntry(
                mDomDistillerServiceAndroid, DomDistillerService.this, entryId);
    }

    public String getUrlForEntry(String entryId) {
        return DomDistillerServiceJni.get().getUrlForEntry(
                mDomDistillerServiceAndroid, DomDistillerService.this, entryId);
    }

    @CalledByNative
    private static DomDistillerService create(long nativeDomDistillerServiceAndroid) {
        ThreadUtils.assertOnUiThread();
        return new DomDistillerService(nativeDomDistillerServiceAndroid);
    }

    @NativeMethods
    interface Natives {
        boolean hasEntry(
                long nativeDomDistillerServiceAndroid, DomDistillerService caller, String entryId);
        String getUrlForEntry(
                long nativeDomDistillerServiceAndroid, DomDistillerService caller, String entryId);
        long getDistilledPagePrefsPtr(long nativeDomDistillerServiceAndroid);
    }
}

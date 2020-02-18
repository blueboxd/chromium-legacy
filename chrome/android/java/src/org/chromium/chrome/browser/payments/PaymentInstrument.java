// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.Drawable;

/** Stub. TODO(rouslan): Remove this file. */
public abstract class PaymentInstrument extends PaymentApp {
    protected PaymentInstrument(String id, String label, String sublabel, Drawable icon) {
        super(id, label, sublabel, icon);
    }

    protected PaymentInstrument(
            String id, String label, String sublabel, String tertiarylabel, Drawable icon) {
        super(id, label, sublabel, tertiarylabel, icon);
    }
}

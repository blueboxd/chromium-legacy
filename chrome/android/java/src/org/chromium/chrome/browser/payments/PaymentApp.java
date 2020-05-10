// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.Drawable;

/** TODO(crbug.com/1081024): Remove this stub once dependencies have been updated. */
public abstract class PaymentApp extends org.chromium.components.payments.PaymentApp {
    protected PaymentApp(String id, String label, String sublabel, Drawable icon) {
        super(id, label, sublabel, icon);
    }

    protected PaymentApp(
            String id, String label, String sublabel, String tertiarylabel, Drawable icon) {
        super(id, label, sublabel, tertiarylabel, icon);
    }

    public interface PaymentRequestUpdateEventListener
            extends org.chromium.components.payments.PaymentApp.PaymentRequestUpdateEventListener {}

    public interface PaymentRequestUpdateEventCallback
            extends org.chromium.components.payments.PaymentApp.PaymentRequestUpdateEventCallback {}
}

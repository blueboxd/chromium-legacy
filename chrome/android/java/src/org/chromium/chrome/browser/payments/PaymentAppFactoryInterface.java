// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

/** Interface for factories that create payment apps. */
public interface PaymentAppFactoryInterface {
    /**
     * Creates payment apps for the |delegate|.
     *
     * @param delegate Provides information about payment request and receives a list of payment
     * apps.
     */
    void create(PaymentAppFactoryDelegate delegate);
}

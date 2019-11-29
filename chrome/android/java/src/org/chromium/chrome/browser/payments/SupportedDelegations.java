// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

/**
 * This class represents the supported delegations of a service worker based payment app.
 */
public class SupportedDelegations {
    private final boolean mShippingAddress;
    private final boolean mPayerName;
    private final boolean mPayerPhone;
    private final boolean mPayerEmail;

    SupportedDelegations(
            boolean shippingAddress, boolean payerName, boolean payerPhone, boolean payerEmail) {
        mShippingAddress = shippingAddress;
        mPayerName = payerName;
        mPayerPhone = payerPhone;
        mPayerEmail = payerEmail;
    }
    SupportedDelegations() {
        mShippingAddress = false;
        mPayerName = false;
        mPayerPhone = false;
        mPayerEmail = false;
    }

    public boolean getShippingAddress() {
        return mShippingAddress;
    }

    public boolean getPayerName() {
        return mPayerName;
    }

    public boolean getPayerPhone() {
        return mPayerPhone;
    }

    public boolean getPayerEmail() {
        return mPayerEmail;
    }
}

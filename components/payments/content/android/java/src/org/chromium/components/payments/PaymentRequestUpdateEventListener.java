// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/**
 * The interface for listener to payment method, shipping address, and shipping option change
 * events. Note: What the spec calls "payment methods" in the context of a "change event", this
 * code calls "apps".
 * TODO(crbug.com/1083242): Move PaymentApp.PaymentRequestUpdateEventListener interface here after
 * updating dependencies.
 */
public interface PaymentRequestUpdateEventListener
        extends PaymentApp.PaymentRequestUpdateEventListener {}

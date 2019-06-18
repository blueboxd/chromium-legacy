// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/**
 * Contains tags used for java UI tests.
 *
 * These tags are used to tag dynamically created views (i.e., without resource ID) to allow tests
 * to easily retrieve and test them.
 */
public class AssistantTagsForTesting {
    public static final String PAYMENT_REQUEST_ACCORDION_TAG = "accordion";
    public static final String PAYMENT_REQUEST_CONTACT_DETAILS_SECTION_TAG = "contact";
    public static final String PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_TAG = "payment";
    public static final String PAYMENT_REQUEST_SHIPPING_ADDRESS_SECTION_TAG = "shipping";
    public static final String VERTICAL_EXPANDER_CHEVRON = "chevron";
}

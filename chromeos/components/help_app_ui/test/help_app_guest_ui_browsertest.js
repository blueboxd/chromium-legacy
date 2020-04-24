// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://help-app. */

// Test that language is set correctly on the guest frame.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en');
});

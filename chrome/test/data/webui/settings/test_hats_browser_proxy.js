// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.HatsBrowserProxy} */
class TestHatsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'tryShowSurvey',
    ]);
  }

  /** @override*/
  tryShowSurvey() {
    this.methodCalled('tryShowSurvey');
  }
}
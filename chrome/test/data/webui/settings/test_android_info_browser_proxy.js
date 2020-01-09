// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('android_info', function() {
  /**
   * Test value for messages for web permissions origin.
   */
  const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

  /** @implements {settings.AndroidInfoBrowserProxy} */
  class TestAndroidInfoBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAndroidSmsInfo',
      ]);
      this.androidSmsInfo = {origin: TEST_ANDROID_SMS_ORIGIN, enabled: true};
    }

    /** @override */
    getAndroidSmsInfo() {
      this.methodCalled('getAndroidSmsInfo');
      return Promise.resolve(this.androidSmsInfo);
    }
  }

  return {
    TestAndroidInfoBrowserProxy: TestAndroidInfoBrowserProxy,
    TEST_ANDROID_SMS_ORIGIN: TEST_ANDROID_SMS_ORIGIN,
  };
});

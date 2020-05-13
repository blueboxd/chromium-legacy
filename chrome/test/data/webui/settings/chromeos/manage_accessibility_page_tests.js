// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('ManageAccessibilityPageTests', function() {
  let page = null;
  let browserProxy = null;

  /** @implements {settings.DevicePageBrowserProxy} */
  class TestDevicePageBrowserProxy {
    constructor() {
      /** @private {boolean} */
      this.hasMouse_ = true;
      /** @private {boolean} */
      this.hasTouchpad_ = true;
    }

    /** @param {boolean} hasMouse */
    set hasMouse(hasMouse) {
      this.hasMouse_ = hasMouse;
      cr.webUIListenerCallback('has-mouse-changed', this.hasMouse_);
    }

    /** @param {boolean} hasTouchpad */
    set hasTouchpad(hasTouchpad) {
      this.hasTouchpad_ = hasTouchpad;
      cr.webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }

    /** @override */
    initializePointers() {
      cr.webUIListenerCallback('has-mouse-changed', this.hasMouse_);
      cr.webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    }
  }

  setup(function() {
    browserProxy = new TestDevicePageBrowserProxy();
    settings.DevicePageBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    page = document.createElement('settings-manage-a11y-page');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Pointers row only visible if mouse/touchpad present', function() {
    const row = page.$$('#pointerSubpageButton');
    assertFalse(row.hidden);

    // Has touchpad, doesn't have mouse ==> not hidden.
    browserProxy.hasMouse = false;
    assertFalse(row.hidden);

    // Doesn't have either ==> hidden.
    browserProxy.hasTouchpad = false;
    assertTrue(row.hidden);

    // Has mouse, doesn't have touchpad ==> not hidden.
    browserProxy.hasMouse = true;
    assertFalse(row.hidden);

    // Has both ==> not hidden.
    browserProxy.hasTouchpad = true;
    assertFalse(row.hidden);
  });
});

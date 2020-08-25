// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * Suite of tests for multidevice-notification-access-setup-dialog element.
 */
suite('Multidevice', () => {
  /** @type {?TestMultideviceBrowserProxy} */
  let browserProxy;

  /** @type {?MultideviceNotificationsOptInDialog} */
  let notificationAccessSetupDialog = null;

  /** @type {?HTMLElement} */
  let buttonContainer = null;

  /**
   * @param {NotificationAccessSetupOperationStatus} status
   */
  function simulateStatusChanged(status) {
    cr.webUIListenerCallback('settings.onNotificationAccessSetupStatusChanged',
        status);
    Polymer.dom.flush();
  }

  setup(() => {
    PolymerTest.clearBody();
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    notificationAccessSetupDialog =
        document.createElement(
            'settings-multidevice-notification-access-setup-dialog');
    document.body.appendChild(notificationAccessSetupDialog);
    Polymer.dom.flush();
    buttonContainer =
        assert(notificationAccessSetupDialog.$$('#buttonContainer'));
  });

  test('Test success flow', async () => {
    // The cancel and confirm buttons should be visible, and ok button hidden.
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#confirmButton'));
    assertFalse(!!buttonContainer.querySelector('#okButton'));
    buttonContainer.querySelector('#confirmButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);

    // The ok and confirm buttons should be hidden, and cancel button visible.
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#confirmButton'));
    assertFalse(!!buttonContainer.querySelector('#okButton'));

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY);

    // The cancel and confirm buttons should be hidden, and ok button visible.
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#confirmButton'));
    assertTrue(!!buttonContainer.querySelector('#okButton'));

    assertTrue(notificationAccessSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#okButton').click();
    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
  });

  test('Test cancel during connecting flow', async () => {
    // The cancel and confirm buttons should be visible, and ok button hidden.
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#confirmButton'));
    assertFalse(!!buttonContainer.querySelector('#okButton'));
    buttonContainer.querySelector('#confirmButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);

    // The ok and confirm buttons should be hidden, and cancel button visible.
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#confirmButton'));
    assertFalse(!!buttonContainer.querySelector('#okButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
  });

});

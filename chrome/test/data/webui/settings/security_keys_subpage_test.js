// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.SecurityKeysPINBrowserProxy} */
class TestSecurityKeysPINBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'startSetPIN',
      'setPIN',
      'close',
    ]);

    /**
     * A map from method names to a promise to return when that method is
     * called. (If no promise is installed, a never-resolved promise is
     * returned.)
     * @private {!Map<string, !Promise>}
     */
    this.promiseMap_ = new Map();
  }

  /**
   * @param {string} methodName
   * @param {!Promise} promise
   */
  setResponseFor(methodName, promise) {
    this.promiseMap_.set(methodName, promise);
  }

  /**
   * @param {string} methodName
   * @param {*} opt_arg
   * @return {!Promise}
   * @private
   */
  handleMethod_(methodName, opt_arg) {
    this.methodCalled(methodName, opt_arg);
    const promise = this.promiseMap_.get(methodName);
    if (promise != undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }

  /** @override */
  startSetPIN() {
    return this.handleMethod_('startSetPIN');
  }

  /** @override */
  setPIN(oldPIN, newPIN) {
    return this.handleMethod_('setPIN', {oldPIN, newPIN});
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

/** @implements {settings.SecurityKeysResetBrowserProxy} */
class TestSecurityKeysResetBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'reset',
      'completeReset',
      'close',
    ]);

    /**
     * A map from method names to a promise to return when that method is
     * called. (If no promise is installed, a never-resolved promise is
     * returned.)
     * @private {!Map<string, !Promise>}
     */
    this.promiseMap_ = new Map();
  }

  /**
   * @param {string} methodName
   * @param {!Promise} promise
   */
  setResponseFor(methodName, promise) {
    this.promiseMap_.set(methodName, promise);
  }

  /**
   * @param {string} methodName
   * @param {*} opt_arg
   * @return {!Promise}
   * @private
   */
  handleMethod_(methodName, opt_arg) {
    this.methodCalled(methodName, opt_arg);
    const promise = this.promiseMap_.get(methodName);
    if (promise != undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }

  /** @override */
  reset() {
    return this.handleMethod_('reset');
  }

  /** @override */
  completeReset() {
    return this.handleMethod_('completeReset');
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

/** @implements {settings.SecurityKeysCredentialBrowserProxy} */
class TestSecurityKeysCredentialBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'startCredentialManagement',
      'providePIN',
      'enumerateCredentials',
      'deleteCredentials',
      'close',
    ]);

    /**
     * A map from method names to a promise to return when that method is
     * called. (If no promise is installed, a never-resolved promise is
     * returned.)
     * @private {!Map<string, !Promise>}
     */
    this.promiseMap_ = new Map();
  }

  /**
   * @param {string} methodName
   * @param {!Promise} promise
   */
  setResponseFor(methodName, promise) {
    this.promiseMap_.set(methodName, promise);
  }

  /**
   * @param {string} methodName
   * @param {*} opt_arg
   * @return {!Promise}
   * @private
   */
  handleMethod_(methodName, opt_arg) {
    this.methodCalled(methodName, opt_arg);
    const promise = this.promiseMap_.get(methodName);
    if (promise != undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }

  /** @override */
  startCredentialManagement() {
    return this.handleMethod_('startCredentialManagement');
  }

  /** @override */
  providePIN(pin) {
    return this.handleMethod_('providePIN', pin);
  }

  /** @override */
  enumerateCredentials() {
    return this.handleMethod_('enumerateCredentials');
  }

  /** @override */
  deleteCredentials(ids) {
    return this.handleMethod_('deleteCredentials', ids);
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysResetDialog', function() {
  let dialog = null;

  setup(function() {
    browserProxy = new TestSecurityKeysResetBrowserProxy();
    settings.SecurityKeysResetBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-reset-dialog');
  });

  function assertShown(expectedID) {
    const allDivs = [
      'initial', 'noReset', 'resetFailed', 'reset2', 'resetSuccess',
      'resetNotAllowed'
    ];
    assertTrue(allDivs.includes(expectedID));

    const allShown =
        allDivs.filter(id => dialog.$[id].className == 'iron-selected');
    assertEquals(allShown.length, 1);
    assertEquals(allShown[0], expectedID);
  }

  function assertComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'OK');
    assertEquals(dialog.$.button.className, 'action-button');
  }

  function assertNotComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.button.className, 'cancel-button');
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown('initial');
    assertNotComplete();
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown('initial');
    assertNotComplete();
    dialog.$.button.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('NotSupported', async function() {
    browserProxy.setResponseFor(
        'reset', Promise.resolve(1 /* INVALID_COMMAND */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('noReset');
  });

  test('ImmediateUnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ImmediateUnknownError', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    const promiseResolver = new PromiseResolver();
    browserProxy.setResponseFor('completeReset', promiseResolver.promise);
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    assertNotComplete();
    assertShown('reset2');
    promiseResolver.resolve(0 /* success */);
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetSuccess');
  });

  test('UnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor('completeReset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ResetRejected', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor(
        'completeReset', Promise.resolve(48 /* NOT_ALLOWED */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetNotAllowed');
  });
});

suite('SecurityKeysSetPINDialog', function() {
  let dialog = null;

  setup(function() {
    browserProxy = new TestSecurityKeysPINBrowserProxy();
    settings.SecurityKeysPINBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-set-pin-dialog');
  });

  function assertShown(expectedID) {
    const allDivs = [
      'initial',
      'noPINSupport',
      'pinPrompt',
      'success',
      'error',
      'locked',
      'reinsert',
    ];
    assertTrue(allDivs.includes(expectedID));

    const allShown =
        allDivs.filter(id => dialog.$[id].className == 'iron-selected');
    assertEquals(allShown.length, 1);
    assertEquals(allShown[0], expectedID);
  }

  function assertComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'OK');
    assertEquals(dialog.$.closeButton.className, 'action-button');
    assertEquals(dialog.$.pinSubmit.hidden, true);
  }

  function assertNotComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.closeButton.className, 'cancel-button');
    assertEquals(dialog.$.pinSubmit.hidden, false);
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startSetPIN');
    assertShown('initial');
    assertNotComplete();
  });

  // Test error codes that are returned immediately.
  for (let testCase of [
           [1 /* INVALID_COMMAND */, 'noPINSupport'],
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('ImmediateError' + testCase[0].toString(), async function() {
      browserProxy.setResponseFor(
          'startSetPIN',
          Promise.resolve([1 /* operation complete */, testCase[0]]));
      document.body.appendChild(dialog);

      await browserProxy.whenCalled('startSetPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(testCase[1]);
      if (testCase[1] == 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ZeroRetries', async function() {
    // Authenticators can also signal that they are locked by indicating zero
    // attempts remaining.
    browserProxy.setResponseFor(
        'startSetPIN',
        Promise.resolve([0 /* not yet complete */, 0 /* no retries */]));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('startSetPIN');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('locked');
  });

  function setPINEntry(inputElement, pinValue) {
    inputElement.value = pinValue;
    // Dispatch input events to trigger validation and UI updates.
    inputElement.dispatchEvent(
        new CustomEvent('input', {bubbles: true, cancelable: true}));
  }

  function setNewPINEntry(pinValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
  }

  function setNewPINEntries(pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    const ret = test_util.eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  function setChangePINEntries(currentPINValue, pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    setPINEntry(dialog.$.currentPIN, currentPINValue);
    const ret = test_util.eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  test('SetPIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    const uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, null /* no current PIN */]);
    await uiReady;
    assertNotComplete();
    assertShown('pinPrompt');
    assertTrue(dialog.$.currentPINEntry.hidden);

    await setNewPINEntries('123', '');
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries('123', '123');
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries('1234', '123');
    assertFalse(dialog.$.newPIN.invalid);
    assertTrue(dialog.$.confirmPIN.invalid);

    const setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setNewPINEntries('1234', '1234');
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
    await browserProxy.whenCalled('close');
    assertShown('success');
    assertComplete();
  });

  // Test error codes that are only returned after attempting to set a PIN.
  for (let testCase of [
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('Error' + testCase[0].toString(), async function() {
      const startSetPINResolver = new PromiseResolver();
      browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
      document.body.appendChild(dialog);
      const uiReady = test_util.eventToPromise('ui-ready', dialog);

      await browserProxy.whenCalled('startSetPIN');
      startSetPINResolver.resolve(
          [0 /* not yet complete */, null /* no current PIN */]);
      await uiReady;

      browserProxy.setResponseFor(
          'setPIN', Promise.resolve([1 /* complete */, testCase[0]]));
      setNewPINEntries('1234', '1234');
      await browserProxy.whenCalled('setPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(testCase[1]);
      if (testCase[1] == 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ChangePIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    let uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, 2 /* two attempts */]);
    await uiReady;
    assertNotComplete();
    assertShown('pinPrompt');
    assertFalse(dialog.$.currentPINEntry.hidden);

    setChangePINEntries('123', '', '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('123', '123', '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('1234', '123', '1234');
    assertFalse(dialog.$.currentPIN.invalid);
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('123', '1234', '1234');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    let setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setPINEntry(dialog.$.currentPIN, '4321');
    setPINEntry(dialog.$.newPIN, '1234');
    setPINEntry(dialog.$.confirmPIN, '1234');
    dialog.$.pinSubmit.click();
    let {oldPIN, newPIN} = await browserProxy.whenCalled('setPIN');
    assertShown('pinPrompt');
    assertNotComplete();
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '4321');
    assertEquals(newPIN, '1234');

    // Simulate an incorrect PIN.
    uiReady = test_util.eventToPromise('ui-ready', dialog);
    setPINResolver.resolve([1 /* complete */, 49 /* incorrect PIN */]);
    await uiReady;
    assertTrue(dialog.$.currentPIN.invalid);
    // Text box for current PIN should not be cleared.
    assertEquals(dialog.$.currentPIN.value, '4321');

    setPINEntry(dialog.$.currentPIN, '43211');

    browserProxy.resetResolver('setPIN');
    setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '43211');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
    await browserProxy.whenCalled('close');
    assertShown('success');
    assertComplete();
  });
});

suite('SecurityKeysCredentialManagement', function() {
  let dialog = null;

  setup(function() {
    browserProxy = new TestSecurityKeysCredentialBrowserProxy();
    settings.SecurityKeysCredentialBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement(
        'settings-security-keys-credential-management-dialog');
  });

  function assertShown(expectedID) {
    const allDivs = ['initial', 'pinPrompt', 'credentials', 'error'];
    assertTrue(allDivs.includes(expectedID));

    const allShown =
        allDivs.filter(id => dialog.$[id].className == 'iron-selected');
    assertEquals(allShown.length, 1);
    assertEquals(allShown[0], expectedID);
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown('initial');
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown('initial');
    dialog.$.cancelButton.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('Finished', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown('initial');
    startResolver.resolve();

    const errorString = 'foo bar baz';
    cr.webUIListenerCallback(
        'security-keys-credential-management-finished', errorString);
    assertShown('error');
    assertTrue(dialog.$.error.textContent.trim().includes(errorString));
  });

  test('Credentials', async function() {
    const startCredentialManagementResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startCredentialManagementResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePIN', pinResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateCredentials', enumerateResolver.promise);
    const deleteResolver = new PromiseResolver();
    browserProxy.setResponseFor('deleteCredentials', deleteResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown('initial');

    // Simulate PIN entry.
    let uiReady = test_util.eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    startCredentialManagementResolver.resolve();
    await uiReady;
    assertShown('pinPrompt');
    dialog.$.pin.value = '0000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '0000');

    // Show a list of three credentials.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateCredentials');
    uiReady = test_util.eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    const credentials = [
      {
        id: 'aaaaaa',
        relyingPartyId: 'acme.com',
        userName: 'userA@example.com',
        userDisplayName: 'User Aaa',
      },
      {
        id: 'bbbbbb',
        relyingPartyId: 'acme.com',
        userName: 'userB@example.com',
        userDisplayName: 'User B',
      },
      {
        id: 'cccccc',
        relyingPartyId: 'acme.com',
        userName: 'userC@example.com',
        userDisplayName: 'User C',
      },
    ];
    enumerateResolver.resolve(credentials);
    await uiReady;
    assertShown('credentials');
    assertEquals(dialog.$.credentialList.items, credentials);

    // Select two of the credentials and delete them.
    Polymer.flush();
    assertTrue(dialog.$.confirmButton.disabled);
    const checkboxes = Array.from(
        Polymer.dom(dialog.$.credentialList).querySelectorAll('cr-checkbox'));
    assertEquals(checkboxes.length, 3);
    assertEquals(checkboxes.filter(el => el.checked).length, 0);
    checkboxes[1].click();
    checkboxes[2].click();
    assertFalse(dialog.$.confirmButton.disabled);

    dialog.$.confirmButton.click();
    const credentialIds = await browserProxy.whenCalled('deleteCredentials');
    assertDeepEquals(credentialIds, ['bbbbbb', 'cccccc']);
    uiReady = test_util.eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    deleteResolver.resolve('foobar' /* localized response message */);
    await uiReady;
    assertShown('error');
    assertTrue(dialog.$.error.textContent.trim().includes('foobar'));
  });
});

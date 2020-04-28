// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/**
 * Creates a single item for the list of passwords.
 * @param {string=} url
 * @param {string=} username
 * @param {number=} id
 * @return {chrome.passwordsPrivate.PasswordUiEntry}
 */
export function createPasswordEntry(url, username, id) {
  // Generate fake data if param is undefined.
  url = url || patternMaker_('www.xxxxxx.com', 16);
  username = username || patternMaker_('user_xxxxx', 16);
  id = id || 0;

  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    username: username,
    id: id,
  };
}

/**
 * Creates a single item for the list of password exceptions.
 * @param {string=} url
 * @param {number=} id
 * @return {chrome.passwordsPrivate.ExceptionEntry}
 */
export function createExceptionEntry(url, id) {
  url = url || patternMaker_('www.xxxxxx.com', 16);
  id = id || 0;
  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
}

/**
 * Creates a new fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
export function createEmptyAddressEntry() {
  return {};
}

/**
 * Creates a fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
export function createAddressEntry() {
  const ret = {};
  ret.guid = makeGuid_();
  ret.fullNames = ['John Doe'];
  ret.companyName = 'Google';
  ret.addressLines = patternMaker_('xxxx Main St', 10);
  ret.addressLevel1 = 'CA';
  ret.addressLevel2 = 'Venice';
  ret.postalCode = patternMaker_('xxxxx', 10);
  ret.countryCode = 'US';
  ret.phoneNumbers = [patternMaker_('(xxx) xxx-xxxx', 10)];
  ret.emailAddresses = [patternMaker_('userxxxx@gmail.com', 16)];
  ret.languageCode = 'EN-US';
  ret.metadata = {isLocal: true};
  ret.metadata.summaryLabel = ret.fullNames[0];
  ret.metadata.summarySublabel = ', ' + ret.addressLines;
  return ret;
}

/**
 * Creates a new empty credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
export function createEmptyCreditCardEntry() {
  const now = new Date();
  const expirationMonth = now.getMonth() + 1;
  const ret = {};
  ret.expirationMonth = expirationMonth.toString();
  ret.expirationYear = now.getFullYear().toString();
  return ret;
}

/**
 * Creates a new random credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
export function createCreditCardEntry() {
  const ret = {};
  ret.guid = makeGuid_();
  ret.name = 'Jane Doe';
  ret.cardNumber = patternMaker_('xxxx xxxx xxxx xxxx', 10);
  ret.expirationMonth = Math.ceil(Math.random() * 11).toString();
  ret.expirationYear = (2016 + Math.floor(Math.random() * 5)).toString();
  ret.metadata = {isLocal: true};
  const cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
  const card = cards[Math.floor(Math.random() * cards.length)];
  ret.metadata.summaryLabel = card + ' ' +
      '****' + ret.cardNumber.substr(-4);
  return ret;
}

/**
 * Creates a new compromised credential.
 * @param {string=} url
 * @param {string=} username
 * @param {string=} type
 * @param {number=} id
 * @param {number=} elapsedMinSinceCompromise
 * @return {chrome.passwordsPrivate.CompromisedCredential}
 * @private
 */
export function makeCompromisedCredential(
    url, username, type, id, elapsedMinSinceCompromise) {
  return {
    id: id,
    formattedOrigin: url,
    changePasswordUrl: `http://${url}/`,
    username: username,
    elapsedTimeSinceCompromise: `${elapsedMinSinceCompromise} minutes ago`,
    compromiseTime: Date.now() - (elapsedMinSinceCompromise * 60000),
    compromiseType: type,
  };
}

/**
 * Creates a new password check status.
 * @param {!chrome.passwordsPrivate.PasswordCheckState} state
 * @param {!number|undefined} checked
 * @param {!number|undefined} remaining
 * @param {string=} lastCheck
 * @return {!chrome.passwordsPrivate.PasswordCheckStatus}
 */
export function makePasswordCheckStatus(state, checked, remaining, lastCheck) {
  return {
    state: state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    alreadyProcessed: checked,
    remainingInQueue: remaining,
    elapsedTimeSinceLastCheck: lastCheck,
  };
}

/**
 * Creates a new random GUID for testing.
 * @return {string}
 * @private
 */
function makeGuid_() {
  return patternMaker_('xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
}

/**
 * Replaces any 'x' in a string with a random number of the base.
 * @param {string} pattern The pattern that should be used as an input.
 * @param {number} base The number base. ie: 16 for hex or 10 for decimal.
 * @return {string}
 * @private
 */
function patternMaker_(pattern, base) {
  return pattern.replace(/x/g, function() {
    return Math.floor(Math.random() * base).toString(base);
  });
}

/**
 * Helper class for creating password-section sub-element from fake data and
 * appending them to the document.
 */
export class PasswordSectionElementFactory {
  /**
   * @param {HTMLDocument} document The test's |document| object.
   */
  constructor(document) {
    this.document = document;
  }

  /**
   * Helper method used to create a password section for the given lists.
   * @param {!PasswordManagerProxy} passwordManager
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
   * @return {!Object}
   */
  createPasswordsSection(passwordManager, passwordList, exceptionList) {
    // Override the PasswordManagerProxy data for testing.
    passwordManager.data.passwords = passwordList;
    passwordManager.data.exceptions = exceptionList;

    // Create a passwords-section to use for testing.
    const passwordsSection = this.document.createElement('passwords-section');
    passwordsSection.prefs = {
      credentials_enable_service: {},
      profile: {
        password_manager_leak_detection: {
          value: true,
        }
      },
    };
    this.document.body.appendChild(passwordsSection);
    flush();
    return passwordsSection;
  }

  /**
   * Helper method used to create a password list item.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordEntry
   * @return {!Object}
   */
  createPasswordListItem(passwordEntry) {
    const passwordListItem = this.document.createElement('password-list-item');
    passwordListItem.item = {entry: passwordEntry, password: ''};
    this.document.body.appendChild(passwordListItem);
    flush();
    return passwordListItem;
  }

  /**
   * Helper method used to create a password editing dialog.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} passwordEntry
   * @return {!Object}
   */
  createPasswordEditDialog(passwordEntry) {
    const passwordDialog = this.document.createElement('password-edit-dialog');
    passwordDialog.item = {entry: passwordEntry, password: ''};
    this.document.body.appendChild(passwordDialog);
    flush();
    return passwordDialog;
  }

  /**
   * Helper method used to create an export passwords dialog.
   * @return {!Object}
   */
  createExportPasswordsDialog(passwordManager) {
    passwordManager.requestExportProgressStatus = callback => {
      callback(chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
    };
    passwordManager.addPasswordsFileExportProgressListener = callback => {
      passwordManager.progressCallback = callback;
    };
    passwordManager.removePasswordsFileExportProgressListener = () => {};
    passwordManager.exportPasswords = (callback) => {
      callback();
    };

    const dialog = this.document.createElement('passwords-export-dialog');
    this.document.body.appendChild(dialog);
    flush();

    return dialog;
  }
}

export class PasswordManagerExpectations {
  constructor() {
    this.requested = {
      passwords: 0,
      exceptions: 0,
      plaintextPassword: 0,
      accountStorageOptInState: 0,
    };

    this.removed = {
      passwords: 0,
      exceptions: 0,
    };

    this.listening = {
      passwords: 0,
      exceptions: 0,
      accountStorageOptInState: 0,
    };
  }
}

/** Helper class to track AutofillManager expectations. */
export class AutofillManagerExpectations {
  constructor() {
    this.requestedAddresses = 0;
    this.listeningAddresses = 0;
  }
}

/**
 * Test implementation
 * @implements {AutofillManager}
 */
export class TestAutofillManager {
  constructor() {
    this.actual_ = new AutofillManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      addresses: [],
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  /** @override */
  setPersonalDataManagerListener(listener) {
    this.actual_.listeningAddresses++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    this.actual_.listeningAddresses--;
  }

  /** @override */
  getAddressList(callback) {
    this.actual_.requestedAddresses++;
    callback(this.data.addresses);
  }

  /**
   * Verifies expectations.
   * @param {!AutofillManagerExpectations} expected
   */
  assertExpectations(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedAddresses, actual.requestedAddresses);
    assertEquals(expected.listeningAddresses, actual.listeningAddresses);
  }
}

/** Helper class to track PaymentsManager expectations. */
export class PaymentsManagerExpectations {
  constructor() {
    this.requestedCreditCards = 0;
    this.listeningCreditCards = 0;
  }
}

/**
 * Test implementation
 * @implements {PaymentsManager}
 */
export class TestPaymentsManager {
  constructor() {
    this.actual_ = new PaymentsManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      creditCards: [],
      upiIds: [],
    };

    // Holds the last callbacks so they can be called when needed.
    this.lastCallback = {
      setPersonalDataManagerListener: null,
    };
  }

  /** @override */
  setPersonalDataManagerListener(listener) {
    this.actual_.listeningCreditCards++;
    this.lastCallback.setPersonalDataManagerListener = listener;
  }

  /** @override */
  removePersonalDataManagerListener(listener) {
    this.actual_.listeningCreditCards--;
  }

  /** @override */
  getCreditCardList(callback) {
    this.actual_.requestedCreditCards++;
    callback(this.data.creditCards);
  }

  /** @override */
  getUpiIdList(callback) {
    this.actual_.requestedUpiIds++;
    callback(this.data.upiIds);
  }

  /**
   * Verifies expectations.
   * @param {!PaymentsManagerExpectations} expected
   */
  assertExpectations(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedCreditCards, actual.requestedCreditCards);
    assertEquals(expected.listeningCreditCards, actual.listeningCreditCards);
  }
}

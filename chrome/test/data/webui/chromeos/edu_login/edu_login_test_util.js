// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * @param {string} email
 * @param {string} displayName
 * @param {string} profileImage
 * @param {string} obfuscatedGaiaId
 * @return {!ParentAccount}
 */
export function getFakeParent(
    email, displayName, profileImage, obfuscatedGaiaId) {
  return {
    email: email,
    displayName: displayName,
    profileImage: profileImage,
    obfuscatedGaiaId: obfuscatedGaiaId,
  };
}

/** @return {!Array<!ParentAccount>} */
export function getFakeParentsList() {
  return [
    getFakeParent('parent1@gmail.com', 'Parent 1', '', 'parent1gaia'),
    getFakeParent('parent2@gmail.com', 'Parent 2', '', 'parent2gaia'),
  ];
}

/** @implements {EduAccountLoginBrowserProxy} */
export class TestEduAccountLoginBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getParents',
      'parentSignin',
      'loginInitialize',
      'authExtensionReady',
      'switchToFullTab',
      'completeLogin',
      'dialogClose',
    ]);

    /** @private {function} */
    this.parentSigninResponse_ = null;
  }

  /** @override */
  getParents() {
    this.methodCalled('getParents');
    return Promise.resolve(getFakeParentsList());
  }

  /** @override */
  parentSignin(parent, password) {
    this.methodCalled('parentSignin', parent, password);
    return this.parentSigninResponse_();
  }

  /** @param {function} parentSigninResponse */
  setParentSigninResponse(parentSigninResponse) {
    this.parentSigninResponse_ = parentSigninResponse;
  }

  /** @override */
  loginInitialize() {
    this.methodCalled('loginInitialize');
  }

  /** @override */
  authExtensionReady() {
    this.methodCalled('authExtensionReady');
  }

  /** @override */
  switchToFullTab(url) {
    this.methodCalled('switchToFullTab', url);
  }

  /** @override */
  completeLogin(credentials, eduLoginParams) {
    this.methodCalled('completeLogin', [credentials, eduLoginParams]);
  }

  /** @override */
  dialogClose() {
    this.methodCalled('dialogClose');
  }
}

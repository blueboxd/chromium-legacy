// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {ParentAccount} from './edu_login_util.js';

/** @interface */
export class EduAccountLoginBrowserProxy {
  /**
   * Send 'getParents' request to the handler. The promise will be resolved
   * with the list of parents (Array<ParentAccount>).
   * @return {Promise<Array<ParentAccount>>}
   */
  getParents() {}

  /**
   * Send parent credentials to get the reauth proof token. The promise will be
   * resolved with the RAPT.
   *
   * @param {ParentAccount} parent
   * @param {String} password parent password
   * @return {Promise<string>}
   */
  parentSignin(parent, password) {}
}

/**
 * @implements {EduAccountLoginBrowserProxy}
 */
export class EduAccountLoginBrowserProxyImpl {
  /** @override */
  getParents() {
    return sendWithPromise('getParents');
  }

  /** @override */
  parentSignin(parent, password) {
    return sendWithPromise('parentSignin', parent, password);
  }
}

addSingletonGetter(EduAccountLoginBrowserProxyImpl);

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @typedef {{id: string, name: string, canBeDisabled: boolean}} */
/* #export */ let NtpExtension;

cr.define('settings', function() {
  /** @interface */
  /* #export */ class OnStartupBrowserProxy {
    /** @return {!Promise<?NtpExtension>} */
    getNtpExtension() {}
  }

  /**
   * @implements {settings.OnStartupBrowserProxy}
   */
  /* #export */ class OnStartupBrowserProxyImpl {
    /** @override */
    getNtpExtension() {
      return cr.sendWithPromise('getNtpExtension');
    }
  }

  cr.addSingletonGetter(OnStartupBrowserProxyImpl);

  // #cr_define_end
  return {
    OnStartupBrowserProxy: OnStartupBrowserProxy,
    OnStartupBrowserProxyImpl: OnStartupBrowserProxyImpl,
  };
});

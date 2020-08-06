// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @fileoverview A helper object used by the time zone subpage page. */
cr.define('settings', function() {
  /** @interface */
  /* #export */ class TimeZoneBrowserProxy {
    /** Notifies C++ code to show parent access code verification view. */
    showParentAccessForTimeZone() {}
  }

  /** @implements {settings.TimeZoneBrowserProxy} */
  /* #export */ class TimeZoneBrowserProxyImpl {
    /** @override */
    showParentAccessForTimeZone() {
      chrome.send('handleShowParentAccessForTimeZone');
    }
  }

  cr.addSingletonGetter(TimeZoneBrowserProxyImpl);
  // #cr_define_end
  return {
    TimeZoneBrowserProxy: TimeZoneBrowserProxy,
    TimeZoneBrowserProxyImpl: TimeZoneBrowserProxyImpl
  };
});

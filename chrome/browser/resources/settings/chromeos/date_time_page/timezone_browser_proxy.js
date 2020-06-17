// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used by the time zone subpage page. */
cr.define('settings', function() {
  /** @interface */
  class TimeZoneBrowserProxy {
    /** Notifies C++ code to show parent access code verification view. */
    showParentAccessForTimeZone() {}
  }

  /** @implements {settings.TimeZoneBrowserProxy} */
  class TimeZoneBrowserProxyImpl {
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

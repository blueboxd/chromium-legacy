// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides functions used for recording user actions within settings.
 * Also provides a way to inject a test implementation for verifying
 * user action recording.
 */
cr.define('settings', function() {
  /** @type {?chromeos.settings.mojom.UserActionRecorderInterface} */
  let userActionRecorder = null;

  /**
   * @param {!chromeos.settings.mojom.UserActionRecorderInterface}
   *     testRecorder
   */
  function setUserActionRecorderForTesting(testRecorder) {
    userActionRecorder = testRecorder;
  }

  /**
   * @return {!chromeos.settings.mojom.UserActionRecorderInterface}
   */
  function getRecorder() {
    if (userActionRecorder) {
      return userActionRecorder;
    }

    userActionRecorder = chromeos.settings.mojom.UserActionRecorder.getRemote();
    return userActionRecorder;
  }

  function recordPageFocus() {
    getRecorder().recordPageFocus();
  }

  function recordPageBlur() {
    getRecorder().recordPageBlur();
  }

  function recordClick() {
    getRecorder().recordClick();
  }

  function recordNavigation() {
    getRecorder().recordNavigation();
  }

  function recordSearch() {
    getRecorder().recordSearch();
  }

  function recordSettingChange() {
    getRecorder().recordSettingChange();
  }

  // #cr_define_end
  return {
    setUserActionRecorderForTesting,
    recordPageFocus,
    recordPageBlur,
    recordClick,
    recordNavigation,
    recordSearch,
    recordSettingChange,
  };
});

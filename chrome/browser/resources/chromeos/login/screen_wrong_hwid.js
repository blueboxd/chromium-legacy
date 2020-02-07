// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

login.createScreen('WrongHWIDScreen', 'wrong-hwid', function() {
  return {
    /** @override */
    decorate() {
      $('skip-hwid-warning-link').addEventListener('click', function(event) {
        chrome.send('wrongHWIDOnSkip');
      });
      this.updateLocalizedContent();
    },

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.WRONG_HWID_WARNING;
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent() {
      $('wrong-hwid-message-content').innerHTML = '<p>' +
          loadTimeData.getStringF(
              'wrongHWIDMessageFirstPart', '<strong>', '</strong>') +
          '</p><p>' + loadTimeData.getString('wrongHWIDMessageSecondPart') +
          '</p>';
    }
  };
});

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Testing stub for messages.
 */

goog.provide('TestMsgs');

goog.require('Msgs');
goog.require('TestMessages');

class TestMsgs {
  constructor() {}

  /**
   * @return {string} The locale.
   */
  static getLocale() {
    return 'testing';
  }

  /**
   * @param {string} messageId
   * @param {Array<string>=} opt_subs
   * @return {string}
   */
  static getMsg(messageId, opt_subs) {
    if (!messageId) {
      throw Error('Message id required');
    }
    var messageString = TestMsgs.Untranslated[messageId.toUpperCase()];
    if (messageString === undefined) {
      var messageObj = TestMessages[('chromevox_' + messageId).toUpperCase()];
      if (messageObj === undefined) {
        throw Error('missing-msg: ' + messageId);
      }
      var messageString = messageObj.message;
      var placeholders = messageObj.placeholders;
      if (placeholders) {
        for (name in placeholders) {
          messageString = messageString.replace(
              '$' + name + '$', placeholders[name].content);
        }
      }
    }
    return Msgs.applySubstitutions_(messageString, opt_subs);
  }
}

/**
 * @type {function(string, Array<string>=): string}
 * @private
 */
TestMsgs.applySubstitutions_ = Msgs.applySubstitutions_;

/**
 * @type {Object<string>}
 */
TestMsgs.Untranslated = Msgs.Untranslated;


/**
 * @param {number} num
 * @return {string}
 */
TestMsgs.getNumber = Msgs.getNumber;

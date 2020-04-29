// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The abstract interface for the CCA's interaction with the browser.
 * @interface
 */
export class BrowserProxy {
  /**
   * @return {!Promise<?Array<!chrome.fileSystem.Volume>>}
   * @abstract
   */
  async getVolumeList() {}

  /**
   * @param {!chrome.fileSystem.RequestFileSystemOptions} options
   * @return {!Promise<?FileSystem>}
   * @abstract
   */
  async requestFileSystem(options) {}

  /**
   * @param {(string|!Array<string>|!Object)} keys
   * @return {!Promise<!Object>}
   * @abstract
   */
  async localStorageGet(keys) {}

  /**
   * @param {!Object<string>} items
   * @return {!Promise}
   * @abstract
   */
  async localStorageSet(items) {}

  /**
   * @param {(string|!Array<string>)} items
   * @return {!Promise}
   * @abstract
   */
  async localStorageRemove(items) {}

  /**
   * @return {!Promise<boolean>}
   * @abstract
   */
  async checkMigrated() {}

  /**
   * @return {!Promise}
   * @abstract
   */
  async doneMigrate() {}

  /**
   * @return {!Promise<string>}
   * @abstract
   */
  async getBoard() {}

  /**
   * @param {string} name
   * @param {Array<string>|string=} substitutions
   * @return {string}
   * @abstract
   */
  getI18nMessage(name, substitutions = undefined) {}

  /**
   * @return {!Promise<boolean>}
   * @abstract
   */
  async isCrashReportingEnabled() {}

  /**
   * @param {!FileEntry} file
   * @return {!Promise}
   * @abstract
   */
  async openGallery(file) {}

  /**
   * @param {string} type
   * @abstract
   */
  openInspector(type) {}

  /**
   * @return {string}
   * @abstract
   */
  getAppId() {}

  /**
   * @return {string}
   * @abstract
   */
  getAppVersion() {}

  /**
   * @param {function(*, !MessageSender, function(string)): (boolean|undefined)}
   *     listener
   * @abstract
   */
  addOnMessageExternalListener(listener) {}

  /**
   * @param {function(Port)} listener
   * @abstract
   */
  addOnConnectExternalListener(listener) {}

  /**
   * @param {string} extensionId
   * @param {*} message
   * @abstract
   */
  sendMessage(extensionId, message) {}

  /**
   * @abstract
   */
  addDummyHistoryIfNotAvailable() {}

  /**
   * @return {boolean}
   * @abstract
   */
  isMp4RecordingEnabled() {}
}

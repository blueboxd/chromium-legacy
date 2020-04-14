// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs for interfaces in //third_party/blink/renderer/modules/launch/*
 * This file can be removed when upstreamed to the closure compiler.
 */

/** @interface */
class FileSystemWriter {
  /**
   * @param {number} position
   * @param {BufferSource|Blob|string} data
   * @return {!Promise<undefined>}
   */
  async write(position, data) {}

  /**
   * @param {number} size
   * @return {!Promise<undefined>}
   */
  async truncate(size) {}

  /**
   * @return {!Promise<undefined>}
   */
  async close() {}
}

/**
 * @typedef {{
 *   type: string,
 *   size: (number|undefined),
 *   position: (number|undefined),
 *   data: (BufferSource|Blob|string|undefined)
 * }}
 */
let WriteParams;

/** @interface */
class FileSystemWritableFileStream {
  /**
   * @param {BufferSource|Blob|string|WriteParams} data
   * @return {!Promise<undefined>}
   */
  async write(data) {}

  /**
   * @param {number} size
   * @return {!Promise<undefined>}
   */
  async truncate(size) {}

  /**
   * @return {!Promise<undefined>}
   */
  async close() {}

  /**
   * @param {number} offset
   * @return {!Promise<undefined>}
   */
  async seek(offset) {}
}

/** @typedef {{writable: boolean}} */
let FileSystemHandlePermissionDescriptor;

/** @interface */
class FileSystemHandle {
  constructor() {
    /** @type {boolean} */
    this.isFile;

    /** @type {boolean} */
    this.isDirectory;

    /** @type {string} */
    this.name;
  }

  /**
   * @param {FileSystemHandle} other
   * @return {!Promise<boolean>}
   */
  isSameEntry(other) {}

  /**
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {!Promise<PermissionState>}
   */
  queryPermission(descriptor) {}

  /**
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {!Promise<PermissionState>}
   */
  requestPermission(descriptor) {}
}

/** @typedef {{keepExistingData: boolean}} */
let FileSystemCreateWriterOptions;

/** @interface */
class FileSystemFileHandle extends FileSystemHandle {
  /**
   * @deprecated TODO(b/151564533): Remove when m82 is stable.
   * @param {FileSystemCreateWriterOptions=} options
   * @return {!Promise<!FileSystemWriter>}
   */
  createWriter(options) {}

  /**
   * @param {FileSystemCreateWriterOptions=} options
   * @return {!Promise<!FileSystemWritableFileStream>}
   */
  createWritable(options) {}

  /** @return {!Promise<!File>} */
  getFile() {}
}

/** @typedef {{create: boolean}} */
let FileSystemGetFileOptions;

/** @typedef {{create: boolean}} */
let FileSystemGetDirectoryOptions;

/** @typedef {{recursive: boolean}} */
let FileSystemRemoveOptions;

/** @typedef {{type: string}} */
let GetSystemDirectoryOptions;

/** @interface */
class FileSystemDirectoryHandle extends FileSystemHandle {
  /**
   * @param {string} name
   * @param {FileSystemGetFileOptions=} options
   * @return {!Promise<!FileSystemFileHandle>}
   */
  getFile(name, options) {}

  /**
   * @param {string} name
   * @param {FileSystemGetDirectoryOptions=} options
   * @return {Promise<!FileSystemDirectoryHandle>}
   */
  getDirectory(name, options) {}

  /** @return {!AsyncIterable<!FileSystemHandle>} */
  getEntries() {}

  /**
   * @param {string} name
   * @param {FileSystemRemoveOptions=} options
   * @return {Promise<undefined>}
   */
  removeEntry(name, options) {}

  /**
   * @param {GetSystemDirectoryOptions} options
   * @return {Promise<!FileSystemDirectoryHandle>}
   */
  static getSystemDirectory(options) {}
}

/** @interface */
class LaunchParams {
  constructor() {
    /** @type {Array<FileSystemHandle>} */
    this.files;

    /** @type {Request} */
    this.request;
  }
}

/** @typedef {function(LaunchParams)} */
let LaunchConsumer;

/** @interface */
class LaunchQueue {
  /** @param {LaunchConsumer} consumer */
  setConsumer(consumer) {}
}

/**
 * @typedef {{
 *    description: (string|undefined),
 *    mimeTypes: (!Array<string>|undefined),
 *    extensions: (!Array<string>|undefined)
 * }}
 */
let ChooseFileSystemEntriesOptionsAccepts;

/**
 * @typedef {{
 *    type: (string|undefined),
 *    multiple: (boolean|undefined),
 *    accepts: (!Array<!ChooseFileSystemEntriesOptionsAccepts>|undefined),
 *    excludeAcceptAllOption: (boolean|undefined)
 * }}
 */
let ChooseFileSystemEntriesOptions;

/**
 * @param {(!ChooseFileSystemEntriesOptions|undefined)} options
 * @return {!Promise<(!FileSystemHandle|!Array<!FileSystemHandle>)>}
 */
window.chooseFileSystemEntries;

/** @type {LaunchQueue} */
window.launchQueue;

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
   */
  async write(position, data) {}

  /**
   * @param {number} size
   */
  async truncate(size) {}

  async close() {}
}

/** @typedef {{writable: boolean}} */
var FileSystemHandlePermissionDescriptor;

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
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {Promise<PermissionState>}
   */
  queryPermission(descriptor) {}

  /**
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {Promise<PermissionState>}
   */
  requestPermission(descriptor) {}
}

/** @typedef {{keepExistingData: boolean}} */
var FileSystemCreateWriterOptions;

/** @interface */
class FileSystemFileHandle extends FileSystemHandle {
  /**
   * @param {FileSystemCreateWriterOptions=} options
   * @return {Promise<!FileSystemWriter>}
   */
  createWriter(options) {}

  /** @return {Promise<!File>} */
  getFile() {}
}

/** @interface */
class LaunchParams {
  constructor() {
    /** @type{Array<FileSystemHandle>} */
    this.files;

    /** @type{Request} */
    this.request;
  }
}

/** @typedef {function(LaunchParams)} */
var LaunchConsumer;

/** @interface */
class LaunchQueue {
  /** @param{LaunchConsumer} consumer */
  setConsumer(consumer) {}
}

/** @type {LaunchQueue} */
window.launchQueue;

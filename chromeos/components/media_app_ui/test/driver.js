// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Host-side of web-driver like controller for sandboxed guest frames. */
class GuestDriver {
  /**
   * Sends a query to the guest that repeatedly runs a query selector until
   * it returns an element.
   *
   * @param {string} query the querySelector to run in the guest.
   * @param {string=} opt_property a property to request on the found element.
   * @param {Object=} opt_commands test commands to execute on the element.
   * @return Promise<string> JSON.stringify()'d value of the property, or
   *   tagName if unspecified.
   */
  async waitForElementInGuest(query, opt_property, opt_commands = {}) {
    /** @type{TestMessageQueryData} */
    const message = {testQuery: query, property: opt_property};

    const result = /** @type{TestMessageResponseData} */ (
        await guestMessagePipe.sendMessage(
            'test', {...message, ...opt_commands}));
    return result.testQueryResult;
  }
}

/** @implements FileSystemWritableFileStream */
class FakeWritableFileStream {
  constructor(/** !Blob= */ data = new Blob()) {
    this.data = data;

    /** @type{function(!Blob)} */
    this.resolveClose;

    this.closePromise = new Promise((/** function(!Blob) */ resolve) => {
      this.resolveClose = resolve;
    });
  }
  /** @override */
  async write(data) {
    const position = 0;  // Assume no seeks.
    this.data = new Blob([
      this.data.slice(0, position),
      data,
      this.data.slice(position + data.size),
    ]);
  }
  /** @override */
  async truncate(size) {
    this.data = this.data.slice(0, size);
  }
  /** @override */
  async close() {
    this.resolveClose(this.data);
  }
  /** @override */
  async seek(offset) {
    throw new Error('seek() not implemented.')
  }
}

/** @implements FileSystemFileHandle  */
class FakeFileSystemFileHandle {
  constructor() {
    this.isFile = true;
    this.isDirectory = false;
    this.name = 'fakefile';

    /** @type{?FakeWritableFileStream} */
    this.lastWritable;
  }

  /** @override */
  queryPermission(descriptor) {}
  /** @override */
  requestPermission(descriptor) {}
  /** @override */
  createWriter(options) {
    throw new Error('createWriter() deprecated.')
  }
  /** @override */
  createWritable(options) {
    this.lastWritable = new FakeWritableFileStream();
    return Promise.resolve(this.lastWritable);
  }
  /** @override */
  getFile() {
    console.error('getFile() not implemented');
    return Promise.resolve(new File([], 'fake-file'));
  }
}

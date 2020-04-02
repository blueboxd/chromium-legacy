// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
 * @type {!Promise<undefined>}
 */
const testMessageHandlersReady = new Promise(resolve => {
  window.addEventListener('DOMContentLoaded', () => {
    guestMessagePipe.registerHandler('test-handlers-ready', resolve);
  });
});

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
    /** @type {TestMessageQueryData} */
    const message = {testQuery: query, property: opt_property};
    await testMessageHandlersReady;
    const result = /** @type {TestMessageResponseData} */ (
        await guestMessagePipe.sendMessage(
            'test', {...message, ...opt_commands}));
    return result.testQueryResult;
  }
}

/**
 * Runs the given `testCase` in the guest context.
 * @param {string} testCase
 */
async function runTestInGuest(testCase) {
  /** @type {TestMessageRunTestCase} */
  const message = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}

/** @implements FileSystemWritableFileStream */
class FakeWritableFileStream {
  constructor(/** !Blob= */ data = new Blob()) {
    this.data = data;

    /** @type {function(!Blob)} */
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
    throw new Error('seek() not implemented.');
  }
}

/** @implements FileSystemHandle  */
class FakeFileSystemHandle {
  constructor() {
    this.isFile = true;
    this.isDirectory = false;
    this.name = 'fake_file.png';
  }
  /** @override */
  async isSameEntry(other) {
    return this === other;
  }
  /** @override */
  async queryPermission(descriptor) {}
  /** @override */
  async requestPermission(descriptor) {}
}

/** @implements FileSystemFileHandle  */
class FakeFileSystemFileHandle extends FakeFileSystemHandle {
  constructor() {
    super();
    /** @type {?FakeWritableFileStream} */
    this.lastWritable;
  }
  /** @override */
  createWriter(options) {
    throw new Error('createWriter() deprecated.');
  }
  /** @override */
  async createWritable(options) {
    this.lastWritable = new FakeWritableFileStream();
    return this.lastWritable;
  }
  /** @override */
  async getFile() {
    // TODO(b/152832337): Use a real image file and set mime type to be
    // 'image/png'. In tests, the src_internal app struggles to reliably load
    // empty images because a size of 0 can't be decoded but also can't reliably
    // load real images due to b/152832025. Mitigate this for now by now by not
    // providing a mime type so the image doesn't get loaded in tests but we can
    // still test the IPC mechanisms.
    return new File([], this.name);
  }
}

/** @implements FileSystemDirectoryHandle  */
class FakeFileSystemDirectoryHandle extends FakeFileSystemHandle {
  constructor() {
    super();
    this.isFile = false;
    this.isDirectory = true;
    this.name = 'fake-dir';
    /**
     * Internal state mocking file handles in a directory handle.
     * @type {!Array<!FakeFileSystemFileHandle>}
     */
    this.files = [];
    /**
     * Used to spy on the last deleted file.
     * @type {?FakeFileSystemFileHandle}
     */
    this.lastDeleted = null;
  }
  /**
   * Use to populate `FileSystemFileHandle`s for tests.
   * @param {!FakeFileSystemFileHandle} fileHandle
   */
  addFileHandleForTest(fileHandle) {
    this.files.push(fileHandle);
  }
  /** @override */
  async getFile(name, options) {
    const fileHandle = this.files.find(f => f.name === name);
    if (!fileHandle && options.create === true) {
      // Simulate creating a new file.
      const newFileHandle = new FakeFileSystemFileHandle();
      newFileHandle.name = name;
      this.files.push(newFileHandle);
      return Promise.resolve(newFileHandle);
    }
    return fileHandle ? Promise.resolve(fileHandle) :
                        Promise.reject((createNamedError(
                            'NotFoundError', `File ${name} not found`)));
  }
  /** @override */
  getDirectory(name, options) {}
  /**
   * @override
   * @return {!AsyncIterable<!FileSystemHandle>}
   * @suppress {reportUnknownTypes} suppress [JSC_UNKNOWN_EXPR_TYPE] for `yield
   * file`.
   */
  async * getEntries() {
    for (const file of this.files) {
      yield file;
    }
  }
  /** @override */
  async removeEntry(name, options) {
    // Remove file handle from internal state.
    const fileHandleIndex = this.files.findIndex(f => f.name === name);
    // Store the file removed for spying in tests.
    this.lastDeleted = this.files.splice(fileHandleIndex, 1)[0];
  }
}

/** Creates a mock directory with a single file in it. */
function createMockTestDirectory() {
  const directory = new FakeFileSystemDirectoryHandle();
  directory.addFileHandleForTest(new FakeFileSystemFileHandle());
  return directory;
}

/**
 * Helper to send a single file to the guest.
 * @param {!File} file
 * @param {!FileSystemFileHandle} handle
 */
function loadFile(file, handle) {
  currentFiles.length = 0;
  currentFiles.push({token: -1, file, handle});
  entryIndex = 0;
  sendFilesToGuest();
}

/**
 * Helper to send multiple file to the guest.
 * @param {!Array<{file: !File, handle: !FileSystemFileHandle}>} files
 * @return {!Promise<undefined>}
 */
function loadMultipleFiles(files) {
  currentFiles.length = 0;
  for (const f of files) {
    currentFiles.push({token: -1, file: f.file, handle: f.handle});
  }
  entryIndex = 0;
  return sendFilesToGuest();
}

/**
 * Creates an `Error` with the name field set.
 * @param {string} name
 * @param {string} msg
 * @return {Error}
 */
function createNamedError(name, msg) {
  const error = new Error(msg);
  error.name = name;
  return error;
}

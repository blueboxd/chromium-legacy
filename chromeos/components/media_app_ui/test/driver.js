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
  /**
   * @param {!string=} name
   */
  constructor(name = 'fake_file.png') {
    this.isFile = true;
    this.isDirectory = false;
    this.name = name;
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
  /**
   * @param {!string=} name
   * @param {!string=} type
   * @param {!Blob} blob
   */
  constructor(name = 'fake_file.png', type = '', blob = new Blob()) {
    super(name);
    this.lastWritable = new FakeWritableFileStream();

    this.lastWritable.data = blob;

    /** @type {!string} */
    this.type = type;
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
    return this.getFileSync();
  }

  /** @return {!File} */
  getFileSync() {
    return new File([this.lastWritable.data], this.name, {type: this.type});
  }
}

/** @implements FileSystemDirectoryHandle  */
class FakeFileSystemDirectoryHandle extends FakeFileSystemHandle {
  /**
   * @param {!string=} name
   */
  constructor(name = 'fake-dir') {
    super(name);
    this.isFile = false;
    this.isDirectory = true;
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
  /**
   * Helper to get all entries as File.
   * @return {!Array<!File>}
   */
  getFilesSync(index) {
    return this.files.map(f => f.getFileSync());
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

/**
 * Structure to define a test file.
 * @typedef{{
 *   name: (string|undefined),
 *   type: (string|undefined),
 *   arrayBuffer: (function(): (Promise<ArrayBuffer>)|undefined)
 * }}
 */
let FileDesc;

/**
 * Creates a mock directory with the provided files in it.
 * @param {!Array<!FileDesc>=} files
 * @return {Promise<FakeFileSystemDirectoryHandle>}
 */
async function createMockTestDirectory(files = [{}]) {
  const directory = new FakeFileSystemDirectoryHandle();
  for (const /** FileDesc */ file of files) {
    const fileBlob = file.arrayBuffer !== undefined ?
        new Blob([await file.arrayBuffer()]) :
        new Blob();
    directory.addFileHandleForTest(
        new FakeFileSystemFileHandle(file.name, file.type, fileBlob));
  }
  return directory;
}

/**
 * Helper to send a single file to the guest.
 * @param {!File} file
 * @param {!FileSystemFileHandle} handle
 * @return {!Promise<undefined>}
 */
async function loadFile(file, handle) {
  currentFiles.length = 0;
  currentFiles.push({token: -1, file, handle});
  entryIndex = 0;
  await sendFilesToGuest();
}

/**
 * Helper to send multiple file to the guest.
 * @param {!Array<{file: !File, handle: !FileSystemFileHandle}>} files
 * @return {!Promise<undefined>}
 */
async function loadMultipleFiles(files) {
  currentFiles.length = 0;
  for (const f of files) {
    currentFiles.push({token: -1, file: f.file, handle: f.handle});
  }
  entryIndex = 0;
  await sendFilesToGuest();
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

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

/**
 * Gets a concatenated list of errors on the currently loaded files. Note the
 * currently open file is always at index 0.
 * @return {!Promise<string>}
 */
async function getFileErrors() {
  const message = {getFileErrors: true};
  const response = /** @type {TestMessageResponseData} */ (
      await guestMessagePipe.sendMessage('test', message));
  return response.testQueryResult;
}

/** @implements FileSystemWritableFileStream */
class FakeWritableFileStream {
  constructor(/** !Blob= */ data = new Blob()) {
    this.data = data;

    /** @type {!Array<!{position: number, size: (number|undefined)}>} */
    this.writes = [];

    /** @type {function(!Blob)} */
    this.resolveClose;

    this.closePromise = new Promise((/** function(!Blob) */ resolve) => {
      this.resolveClose = resolve;
    });
  }
  /** @override */
  async write(data) {
    const position = 0;  // Assume no seeks.
    this.writes.push({position, size: data.size});
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
   * @param {number=} lastModified
   * @param {!Blob} blob
   */
  constructor(
      name = 'fake_file.png', type = '', lastModified = 0, blob = new Blob()) {
    super(name);
    this.lastWritable = new FakeWritableFileStream();

    this.lastWritable.data = blob;

    /** @type {!string} */
    this.type = type;

    /** @type {!number} */
    this.lastModified = lastModified;

    /** @type {?DOMException} */
    this.nextCreateWritableError;
  }
  /** @override */
  createWriter(options) {
    throw new Error('createWriter() deprecated.');
  }
  /** @override */
  async createWritable(options) {
    if (this.nextCreateWritableError) {
      throw this.nextCreateWritableError;
    }
    this.lastWritable = new FakeWritableFileStream();
    return this.lastWritable;
  }
  /** @override */
  async getFile() {
    return this.getFileSync();
  }

  /** @return {!File} */
  getFileSync() {
    return new File(
        [this.lastWritable.data], this.name,
        {type: this.type, lastModified: this.lastModified});
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
  getFilesSync() {
    return this.files.map(f => f.getFileSync());
  }
  /** @override */
  async getFile(name, options) {
    const fileHandle = this.files.find(f => f.name === name);
    if (!fileHandle && options.create === true) {
      // Simulate creating a new file, assume it is an image. This is needed for
      // renaming files to ensure it has the right mime type, the real
      // implementation copies the mime type from the binary.
      const newFileHandle = new FakeFileSystemFileHandle(name, 'image/png');
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
 *   lastModified: (number|undefined),
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
    directory.addFileHandleForTest(new FakeFileSystemFileHandle(
        file.name, file.type, file.lastModified, fileBlob));
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
 * Creates a mock LaunchParams object from the provided `files`.
 * @param {!Array<FileSystemHandle>} files
 * @return {LaunchParams}
 */
function handlesToLaunchParams(files) {
  return /** @type{LaunchParams} */ ({files});
}

/**
 * Helper to "launch" with the given `directoryContents`. Populates a fake
 * directory containing those handles, then launches the app. The focus file is
 * either the first file in `multiSelectionFiles`, or the first directory entry.
 * @param {!Array<!FakeFileSystemFileHandle>} directoryContents
 * @param {!Array<!FakeFileSystemFileHandle>=} multiSelectionFiles If provided,
 *     holds additional files selected in the files app at launch time.
 * @return {!Promise<FakeFileSystemDirectoryHandle>}
 */
async function launchWithHandles(directoryContents, multiSelectionFiles = []) {
  /** @type {?FakeFileSystemFileHandle} */
  let focusFile = multiSelectionFiles[0];
  if (!focusFile) {
    focusFile = directoryContents[0];
  }
  multiSelectionFiles = multiSelectionFiles.slice(1);
  const directory = new FakeFileSystemDirectoryHandle();
  for (const handle of directoryContents) {
    directory.addFileHandleForTest(handle);
  }
  const files = [directory, focusFile, ...multiSelectionFiles];
  await launchConsumer(handlesToLaunchParams(files));
  return directory;
}

/**
 * Wraps a file in a FakeFileSystemFileHandle.
 * @param {!File} file
 * @return {!FakeFileSystemFileHandle}
 */
function fileToFileHandle(file) {
  return new FakeFileSystemFileHandle(
      file.name, file.type, file.lastModified, file);
}

/**
 * Helper to invoke launchWithHandles after wrapping `files` in fake handles.
 * @param {!Array<!File>} files
 * @param {!Array<!number>=} selectedIndexes
 * @return {!Promise<FakeFileSystemDirectoryHandle>}
 */
async function launchWithFiles(files, selectedIndexes = []) {
  const fileHandles = files.map(fileToFileHandle);
  const selection =
      selectedIndexes.map((/** @type {number} */ i) => fileHandles[i]);
  return launchWithHandles(fileHandles, selection);
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

/**
 * @param {!FileSystemDirectoryHandle} directory
 * @param {!File} file
 */
async function loadFilesWithoutSendingToGuest(directory, file) {
  const handle = await directory.getFile(file.name);
  globalLaunchNumber++;
  setCurrentDirectory(directory, {file, handle});
  await processOtherFilesInDirectory(directory, file, globalLaunchNumber);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of files as `expectedFiles`.
 * @param {!Array<!File>} expectedFiles
 * @param {?string} testCase
 */
function assertFilesToBe(expectedFiles, testCase) {
  return assertFilenamesToBe(expectedFiles.map(f => f.name).join(), testCase);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of filenames as `expectedFilenames`.
 * @param {string} expectedFilenames
 * @param {?string} testCase
 */
function assertFilenamesToBe(expectedFilenames, testCase) {
  // Use filenames as an approximation of file uniqueness.
  const currentFilenames = currentFiles.map(d => d.handle.name).join();
  chai.assert.equal(
      currentFilenames, expectedFilenames,
      `Expected '${expectedFilenames}' but got '${currentFilenames}'` +
          (testCase ? ` for ${testCase}` : ''));
}

/**
 * Wraps `chai.assert.match` allowing tests to use `assertMatch`.
 * @param {string} string the string to match
 * @param {string} regex an escaped regex compatible string
 * @param {string|undefined} opt_message logged if the assertion fails
 */
function assertMatch(string, regex, opt_message) {
  chai.assert.match(string, new RegExp(regex), opt_message);
}

/**
 * Use to match error stack traces.
 * @param {string} stackTrace the stacktrace
 * @param {Array<string>} regexLines a list of escaped regex compatible strings,
 *     used to compare with the stacktrace.
 * @param {string|undefined} opt_message logged if the assertion fails
 */
function assertMatchErrorStack(stackTrace, regexLines, opt_message) {
  const regex = `(.|\\n)*${regexLines.join('(.|\\n)*')}(.|\\n)*`;
  assertMatch(stackTrace, regex, opt_message);
}

/**
 * Returns the files loaded in the most recent call to `loadFiles()`.
 * @return {Promise<?Array<!ReceivedFile>>}
 */
async function getLoadedFiles() {
  const response = /** @type {LastLoadedFilesResponse} */ (
      await guestMessagePipe.sendMessage('get-last-loaded-files'));
  if (response.fileList) {
    return response.fileList;
  }
  return null;
}

/**
 * Puts the app into valid but "unexpected" state for it to be in after handling
 * a launch. Currently this restores part of the app state to what it would be
 * on a launch from the icon (i.e. no launch files).
 */
function simulateLosingAccessToDirectory() {
  currentDirectoryHandle = null;
}

/**
 * @param {!FakeFileSystemDirectoryHandle} directory
 */
function launchWithFocusFile(directory) {
  const focusFile = {
    handle: directory.files[0],
    file: directory.files[0].getFileSync()
  };
  globalLaunchNumber++;
  setCurrentDirectory(directory, focusFile);
  return focusFile;
}

/**
 * @param {!FakeFileSystemDirectoryHandle} directory
 * @param {number} totalFiles
 */
async function assertSingleFileLaunch(directory, totalFiles) {
  chai.assert.equal(1, currentFiles.length);

  await sendFilesToGuest();

  const loadedFiles = await getLoadedFiles();
  // The untrusted context only loads the first file.
  chai.assert.equal(1, loadedFiles.length);
  // All files are in the `FileSystemDirectoryHandle`.
  chai.assert.equal(totalFiles, directory.files.length);
}

/**
 * Check files loaded in the trusted context `currentFiles` against the working
 * directory and the untrusted context.
 * @param {!FakeFileSystemDirectoryHandle} directory
 * @param {!Array<string>} fileNames
 * @param {?string} testCase
 */
async function assertFilesLoaded(directory, fileNames, testCase) {
  chai.assert.equal(fileNames.length, directory.files.length);
  chai.assert.equal(fileNames.length, currentFiles.length);

  const loadedFiles = /** @type {!Array<!File>} */ (await getLoadedFiles());
  chai.assert.equal(fileNames.length, loadedFiles.length);

  // Check `currentFiles` in the trusted context matches up with files sent
  // to guest.
  assertFilenamesToBe(fileNames.join(), testCase);
  assertFilesToBe(loadedFiles, testCase);
}

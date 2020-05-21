// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://media-app.
 */
GEN('#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"');

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "third_party/blink/public/common/features.h"');

const HOST_ORIGIN = 'chrome://media-app';
const GUEST_ORIGIN = 'chrome-untrusted://media-app';

/**
 * Regex to match against text of a "generic" error. This just checks for
 * something message-like (a string with at least one letter). Note we can't
 * update error messages in the same patch as this test currently. See
 * https://crbug.com/1080473.
 */
const GENERIC_ERROR_MESSAGE_REGEX = '^".*[A-Za-z].*"$';

let driver = null;

/**
 * Runs a CSS selector until it detects the "error" UX being loaded.
 * @return {!Promise<string>} alt= text of the element showing the error.
 */
function waitForErrorUX() {
  const ERROR_UX_SELECTOR = 'img[src^="/assets/error.png"]';
  return driver.waitForElementInGuest(ERROR_UX_SELECTOR, 'alt');
}

/**
 * Runs a CSS selector that waits for an image to load with the given alt= text
 * and returns its width.
 * @param {string} altText
 * @return {!Promise<string>} The value of the width attribute.
 */
function waitForImageAndGetWidth(altText) {
  return driver.waitForElementInGuest(`img[alt="${altText}"]`, 'naturalWidth');
}

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MediaAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ui/webui/resources/js/assert.js',
      '//chromeos/components/media_app_ui/test/driver.js',
    ];
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get featureList() {
    // NativeFileSystem and FileHandling flags should be automatically set by
    // origin trials when the Media App feature is enabled, but this testing
    // environment does not seem to recognize origin trials, so they must be
    // explicitly set with flags to prevent tests crashing on Media App load due
    // to window.launchQueue being undefined. See http://crbug.com/1071320.
    return {
      enabled: [
        'chromeos::features::kMediaApp',
        'blink::features::kNativeFileSystemAPI',
        'blink::features::kFileHandlingAPI'
      ]
    };
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get typedefCppFixture() {
    return 'MediaAppUiBrowserTest';
  }

  /** @override */
  setUp() {
    super.setUp();
    driver = new GuestDriver();
  }
};

// Give the test image an unusual size so we can distinguish it form other <img>
// elements that may appear in the guest.
const TEST_IMAGE_WIDTH = 123;
const TEST_IMAGE_HEIGHT = 456;

/**
 * @param {number=} width
 * @param {number=} height
 * @param {string=} name
 * @return {Promise<File>} A {width}x{height} transparent encoded image/png.
 */
async function createTestImageFile(
    width = TEST_IMAGE_WIDTH, height = TEST_IMAGE_HEIGHT,
    name = 'test_file.png') {
  const canvas = new OffscreenCanvas(width, height);
  canvas.getContext('2d');  // convertToBlob fails without a rendering context.
  const blob = await canvas.convertToBlob();
  return new File([blob], name, {type: 'image/png'});
}

// Tests that chrome://media-app is allowed to frame
// chrome-untrusted://media-app. The URL is set in the html. If that URL can't
// load, test this fails like JS ERROR: "Refused to frame '...' because it
// violates the following Content Security Policy directive: "frame-src
// chrome-untrusted://media-app/". This test also fails if the guest renderer is
// terminated, e.g., due to webui performing bad IPC such as network requests
// (failure detected in content/public/test/no_renderer_crashes_assertion.cc).
TEST_F('MediaAppUIBrowserTest', 'GuestCanLoad', async () => {
  const guest = document.querySelector('iframe');
  const app = await driver.waitForElementInGuest('backlight-app', 'tagName');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/app.html');
  assertEquals(app, '"BACKLIGHT-APP"');

  testDone();
});

// Tests that we have localized information in the HTML like title and lang.
TEST_F('MediaAppUIBrowserTest', 'HasTitleAndLang', () => {
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Gallery');
  testDone();
});

// Tests that regular launch for an image succeeds.
TEST_F('MediaAppUIBrowserTest', 'LaunchFile', async () => {
  await launchWithFiles([await createTestImageFile()]);
  const result =
      await driver.waitForElementInGuest('img[src^="blob:"]', 'naturalWidth');

  assertEquals(`${TEST_IMAGE_WIDTH}`, result);
  assertEquals(currentFiles.length, 1);
  assertEquals(await getFileErrors(), '');
  testDone();
});

// Tests that we show error UX when trying to launch an unopenable file.
TEST_F('MediaAppUIBrowserTest', 'LaunchUnopenableFile', async () => {
  const mockFileHandle =
      new FakeFileSystemFileHandle('not_allowed.png', 'image/png');
  mockFileHandle.getFileSync = () => {
    throw new DOMException(
        'Fake NotAllowedError for LoadUnopenableFile test.', 'NotAllowedError');
  };
  await launchWithHandles([mockFileHandle]);
  const result = await waitForErrorUX();

  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 0);
  assertEquals(await getFileErrors(), 'NotAllowedError');
  testDone();
});

// Tests that unopenable files in the same directory are ignored at launch.
TEST_F('MediaAppUIBrowserTest', 'LaunchWithUnopenableSibling', async () => {
  const validHandle =
      fileToFileHandle(await createTestImageFile(123, 456, 'allowed.png'));
  const notAllowedHandle =
      new FakeFileSystemFileHandle('not_allowed.png', 'image/png');
  notAllowedHandle.getFileSync = () => {
    throw new DOMException(
        'Fake NotAllowedError for LaunchWithUnopenableSibling test.',
        'NotAllowedError');
  };

  await launchWithHandles([validHandle, notAllowedHandle]);
  const result = await waitForImageAndGetWidth('allowed.png');

  assertEquals(`${TEST_IMAGE_WIDTH}`, result);
  assertEquals(currentFiles.length, 1);  // Unopenable file ignored at launch.
  assertEquals(currentFiles[0].handle.name, 'allowed.png');
  assertEquals(await getFileErrors(), '');  // Ignored => no errors.
  testDone();
});

// Tests that a file that becomes inaccessible after the initial app launch is
// ignored on navigation, and shows an error when navigated to itself.
TEST_F('MediaAppUIBrowserTest', 'NavigateWithUnopenableSibling', async () => {
  const handles = [
    fileToFileHandle(await createTestImageFile(111 /* width */, 10, '1.png')),
    fileToFileHandle(await createTestImageFile(222 /* width */, 10, '2.png')),
    fileToFileHandle(await createTestImageFile(333 /* width */, 10, '3.png')),
  ];

  await launchWithHandles(handles);
  let result = await waitForImageAndGetWidth('1.png');
  assertEquals(result, '111');
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), ',,');

  // Now that we've launched, make the *last* handle unopenable. This is only
  // interesting if we know the file will be re-opened, so check that first.
  // Note that if the file is non-null, no "reopen" occurs: launch.js does not
  // open files a second time after examining siblings for relevance to the
  // focus file.
  assertEquals(currentFiles[2].file, null);
  handles[2].getFileSync = () => {
    throw new DOMException(
        'Fake NotAllowedError for NavigateToUnopenableSibling test.',
        'NotAllowedError');
  };
  await advance(1);  // Navigate to the still-openable second file.

  result = await waitForImageAndGetWidth('2.png');
  assertEquals(result, '222');
  assertEquals(currentFiles.length, 3);

  // The error stays on the third, now unopenable. But, since we've advanced,
  // it has now rotated into the second slot.
  assertEquals(await getFileErrors(), ',NotAllowedError,');

  // Navigate to the unopenable file and expect a graceful error.
  await advance(1);
  result = await waitForErrorUX();
  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), 'NotAllowedError,,');

  // Navigating back to an openable file should still work.
  await advance(1);
  result = await waitForImageAndGetWidth('1.png');
  assertEquals(result, '111');
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), ',,NotAllowedError');

  testDone();
});

// Tests a hypothetical scenario where a file may be deleted and replaced with
// an openable directory with the same name while the app is running.
TEST_F('MediaAppUIBrowserTest', 'FileThatBecomesDirectory', async () => {
  const handles = [
    fileToFileHandle(await createTestImageFile(111 /* width */, 10, '1.png')),
    fileToFileHandle(await createTestImageFile(222 /* width */, 10, '2.png')),
  ];

  await launchWithHandles(handles);
  let result = await waitForImageAndGetWidth('1.png');
  assertEquals(await getFileErrors(), ',');

  handles[1].isFile = false;
  handles[1].isDirectory = true;
  handles[1].getFileSync = () => {
    throw new Error(
        '(in test) FileThatBecomesDirectory: getFileSync should not be called');
  };

  await advance(1);
  result = await waitForErrorUX();
  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 2);
  assertEquals(await getFileErrors(), 'NotAFile,');

  testDone();
});

// Tests that chrome://media-app can successfully send a request to open the
// feedback dialog and receive a response.
TEST_F('MediaAppUIBrowserTest', 'CanOpenFeedbackDialog', async () => {
  const result = await mediaAppPageHandler.openFeedbackDialog();

  assertEquals(result.errorMessage, '');
  testDone();
});

// Tests that video elements in the guest can be full-screened.
TEST_F('MediaAppUIBrowserTest', 'CanFullscreenVideo', async () => {
  // Remove `overflow: hidden` to work around a spurious DCHECK in Blink
  // layout. See crbug.com/1052791. Oddly, even though the video is in the guest
  // iframe document (which also has these styles on its body), it is necessary
  // and sufficient to remove these styles applied to the main frame.
  document.body.style.overflow = 'unset';

  // Load a zero-byte video. It won't load, but the video element should be
  // added to the DOM (and it can still be fullscreened).
  await loadFile(
      new File([], 'zero_byte_video.webm', {type: 'video/webm'}),
      new FakeFileSystemFileHandle());

  const SELECTOR = 'video';
  const tagName = await driver.waitForElementInGuest(
      SELECTOR, 'tagName', {pathToRoot: ['backlight-video-container']});
  const result = await driver.waitForElementInGuest(
      SELECTOR, undefined,
      {pathToRoot: ['backlight-video-container'], requestFullscreen: true});

  // A TypeError of 'fullscreen error' results if fullscreen fails.
  assertEquals(result, 'hooray');
  assertEquals(tagName, '"VIDEO"');

  testDone();
});

// Tests that we receive an error if our message is unhandled.
TEST_F('MediaAppUIBrowserTest', 'ReceivesNoHandlerError', async () => {
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  let caughtError = {};

  try {
    await guestMessagePipe.sendMessage('unknown-message', null);
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(
      caughtError.message,
      'unknown-message: No handler registered for message type \'unknown-message\'');

  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the test context.
    'Error: unknown-message: No handler registered for message type \'unknown-message\'',
    'at MessagePipe.sendMessage \\(chrome:',
    'at async MediaAppUIBrowserTest.<anonymous>',
    // Error stack of the untrusted context (guestMessagePipe) is appended.
    'Error from chrome-untrusted:',
    'Error: No handler registered for message type \'unknown-message\'',
    'at MessagePipe.receiveMessage_ \\(chrome-untrusted:',
    'at MessagePipe.messageListener_ \\(chrome-untrusted:',
  ]);
  testDone();
});

// Tests that we receive an error if the handler fails.
TEST_F('MediaAppUIBrowserTest', 'ReceivesProxiedError', async () => {
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  let caughtError = {};

  try {
    await guestMessagePipe.sendMessage('bad-handler', null);
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(caughtError.message, 'bad-handler: This is an error');
  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the test context.
    'Error: bad-handler: This is an error',
    'at MessagePipe.sendMessage \\(chrome:',
    'at async MediaAppUIBrowserTest.<anonymous>',
    // Error stack of the untrusted context (guestMessagePipe) is appended.
    'Error from chrome-untrusted:',
    'Error: This is an error',
    'at guest_query_receiver.js',
    'at MessagePipe.callHandlerForMessageType_ \\(chrome-untrusted:',
    'at MessagePipe.receiveMessage_ \\(chrome-untrusted:',
    'at MessagePipe.messageListener_ \\(chrome-untrusted:',
  ]);
  testDone();
});

// Tests the IPC behind the implementation of ReceivedFile.overwriteOriginal()
// in the untrusted context. Ensures it correctly updates the file handle owned
// by the privileged context.
TEST_F('MediaAppUIBrowserTest', 'OverwriteOriginalIPC', async () => {
  const handle = new FakeFileSystemFileHandle();
  await loadFile(await createTestImageFile(), handle);

  // Write should not be called initially.
  assertEquals(0, handle.lastWritable.data.size);

  const message = {overwriteLastFile: 'Foo'};
  const testResponse = await guestMessagePipe.sendMessage('test', message);
  const writeResult = await handle.lastWritable.closePromise;

  assertEquals(testResponse.testQueryResult, 'overwriteOriginal resolved');
  assertEquals(await writeResult.text(), 'Foo');
  testDone();
});

// Tests `MessagePipe.sendMessage()` properly propagates errors and appends
// stacktraces.
TEST_F('MediaAppUIBrowserTest', 'CrossContextErrors', async () => {
  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;

  const handle = new FakeFileSystemFileHandle();
  await loadFile(await createTestImageFile(), handle);
  // Force Error by forcing `fileToken` in launch.js to be out of sync.
  fileToken = -1;
  let caughtError = {};

  try {
    const message = {overwriteLastFile: 'Foo'};
    await guestMessagePipe.sendMessage('test', message);
  } catch (e) {
    caughtError = e;
  }

  assertEquals(caughtError.name, 'Error');
  assertEquals(caughtError.message, 'test: overwrite-file: File not current.');
  assertMatchErrorStack(caughtError.stack, [
    // Error stack of the untrusted & test context.
    'at MessagePipe.sendMessage \\(chrome-untrusted:',
    'at async ReceivedFile.overwriteOriginal \\(chrome-untrusted:',
    'at async runTestQuery \\(guest_query_receiver',
    'at async MessagePipe.callHandlerForMessageType_ \\(chrome-untrusted:',
    // Error stack of the trusted context is appended.
    'Error from chrome:',
    'Error: File not current.\\n    at chrome:',
    'at MessagePipe.callHandlerForMessageType_ \\(chrome:',
    'at MessagePipe.receiveMessage_ \\(chrome:',
    'at MessagePipe.messageListener_ \\(chrome:',
  ]);
  testDone();
});

// Tests the IPC behind the implementation of ReceivedFile.deleteOriginalFile()
// in the untrusted context.
TEST_F('MediaAppUIBrowserTest', 'DeleteOriginalIPC', async () => {
  let directory = await launchWithFiles(
      [await createTestImageFile(1, 1, 'first_file_name.png')]);
  const testHandle = directory.files[0];
  let testResponse;

  // Nothing should be deleted initially.
  assertEquals(null, directory.lastDeleted);

  const messageDelete = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDelete);

  // Assertion will fail if exceptions from launch.js are thrown, no exceptions
  // indicates the file was successfully deleted.
  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandle, directory.lastDeleted);
  // File removed from `DirectoryHandle` internal state.
  assertEquals(0, directory.files.length);

  // Load another file and replace its handle in the underlying
  // `FileSystemDirectoryHandle`. This gets us into a state where the file on
  // disk has been deleted and a new file with the same name replaces it without
  // updating the `FakeSystemDirectoryHandle`. The new file shouldn't be deleted
  // as it has a different `FileHandle` reference.
  directory = await launchWithFiles(
      [await createTestImageFile(1, 1, 'first_file_name.png')]);
  directory.files[0] = new FakeFileSystemFileHandle('first_file_name.png');

  // Try delete the first file again, should result in file moved.
  const messageDeleteMoved = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDeleteMoved);

  assertEquals(
      'deleteOriginalFile resolved file moved', testResponse.testQueryResult);
  // New file not removed from `DirectoryHandle` internal state.
  assertEquals(1, directory.files.length);

  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;
  // Test UNKNOWN_ERROR case by not having a directory handle.
  currentDirectoryHandle = undefined;

  const messageDeleteNoOp = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDeleteNoOp);

  assertEquals(
      'deleteOriginalFile resolved UNKNOWN_ERROR',
      testResponse.testQueryResult);
  testDone();
});

// Tests when a file is deleted, the app tries to open the next available file
// and reloads with those files.
TEST_F('MediaAppUIBrowserTest', 'DeletionOpensNextFile', async () => {
  const testFiles = [
    await createTestImageFile(1, 1, 'test_file_1.png'),
    await createTestImageFile(1, 1, 'test_file_2.png'),
    await createTestImageFile(1, 1, 'test_file_3.png'),
  ];
  const directory = await launchWithFiles(testFiles);
  let testResponse;
  // Shallow copy so mutations to `directory.files` don't effect
  // `testHandles`.
  const testHandles = [...directory.files];

  // Check the app loads all 3 files.
  let lastLoadedFiles = await getLoadedFiles();
  assertEquals(3, lastLoadedFiles.length);
  assertEquals('test_file_1.png', lastLoadedFiles[0].name);
  assertEquals('test_file_2.png', lastLoadedFiles[1].name);
  assertEquals('test_file_3.png', lastLoadedFiles[2].name);

  // Delete the first file.
  const messageDelete = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDelete);

  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandles[0], directory.lastDeleted);
  assertEquals(directory.files.length, 2);

  // Check the app reloads the file list with the remaining two files.
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(2, lastLoadedFiles.length);
  assertEquals('test_file_2.png', lastLoadedFiles[0].name);
  assertEquals('test_file_3.png', lastLoadedFiles[1].name);

  // Navigate to the last file (originally the third file) and delete it
  await guestMessagePipe.sendMessage('test', {navigate: 'next'});
  testResponse = await guestMessagePipe.sendMessage('test', messageDelete);

  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandles[2], directory.lastDeleted);
  assertEquals(directory.files.length, 1);

  // Check the app reloads the file list with the last remaining file
  // (originally the second file).
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(1, lastLoadedFiles.length);
  assertEquals(testFiles[1].name, lastLoadedFiles[0].name);

  // Delete the last file, should lead to zero state.
  testResponse = await guestMessagePipe.sendMessage('test', messageDelete);
  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);

  // The app should be in zero state with no media loaded.
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(0, lastLoadedFiles.length);

  testDone();
});

// Tests the IPC behind the loadNext and loadPrev functions on the received file
// list in the untrusted context.
TEST_F('MediaAppUIBrowserTest', 'NavigateIPC', async () => {
  async function fakeEntry() {
    const file = await createTestImageFile();
    const handle = new FakeFileSystemFileHandle(file.name, file.type, file);
    return {file, handle};
  }
  await loadMultipleFiles([await fakeEntry(), await fakeEntry()]);
  assertEquals(entryIndex, 0);

  let result = await guestMessagePipe.sendMessage('test', {navigate: 'next'});
  assertEquals(result.testQueryResult, 'loadNext called');
  assertEquals(entryIndex, 1);

  result = await guestMessagePipe.sendMessage('test', {navigate: 'prev'});
  assertEquals(result.testQueryResult, 'loadPrev called');
  assertEquals(entryIndex, 0);

  result = await guestMessagePipe.sendMessage('test', {navigate: 'prev'});
  assertEquals(result.testQueryResult, 'loadPrev called');
  assertEquals(entryIndex, 1);
  testDone();
});

// Tests the IPC behind the implementation of ReceivedFile.renameOriginalFile()
// in the untrusted context.
TEST_F('MediaAppUIBrowserTest', 'RenameOriginalIPC', async () => {
  const firstFile = await createTestImageFile();
  const directory = await createMockTestDirectory([firstFile]);
  // Simulate steps taken to load a file via a launch event.
  const firstFileHandle = directory.files[0];
  await loadFile(firstFile, firstFileHandle);
  // Set `currentDirectoryHandle` in launch.js.
  currentDirectoryHandle = directory;
  let testResponse;

  // Nothing should be deleted initially.
  assertEquals(null, directory.lastDeleted);

  // Test normal rename flow.
  const messageRename = {renameLastFile: 'new_file_name.png'};
  testResponse = await guestMessagePipe.sendMessage('test', messageRename);

  assertEquals(
      testResponse.testQueryResult, 'renameOriginalFile resolved success');
  // The original file that was renamed got deleted.
  assertEquals(firstFileHandle, directory.lastDeleted);
  // There is still one file which is the renamed version of the original file.
  assertEquals(directory.files.length, 1);
  assertEquals(directory.files[0].name, 'new_file_name.png');
  // Check the new file written has the correct data.
  const newHandle = directory.files[0];
  const newFile = await newHandle.getFile();
  assertEquals(newFile.size, firstFile.size);
  assertEquals(await newFile.text(), await firstFile.text());

  // Test renaming when a file with the new name already exists.
  const messageRenameExists = {renameLastFile: 'new_file_name.png'};
  testResponse =
      await guestMessagePipe.sendMessage('test', messageRenameExists);

  assertEquals(
      testResponse.testQueryResult, 'renameOriginalFile resolved file exists');
  // No change to the existing file.
  assertEquals(directory.files.length, 1);
  assertEquals(directory.files[0].name, 'new_file_name.png');

  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;
  // Test UNKNOWN_ERROR case by making files out of sync.
  fileToken = -1;

  const messageRenameNoOp = {renameLastFile: 'new_file_name_2.png'};
  testResponse = await guestMessagePipe.sendMessage('test', messageRenameNoOp);

  assertEquals(
      testResponse.testQueryResult,
      'renameOriginalFile resolved UNKNOWN_ERROR');
  testDone();
});

// Tests the IPC behind the saveCopy delegate function.
TEST_F('MediaAppUIBrowserTest', 'SaveCopyIPC', async () => {
  // Mock out choose file system entries since it can only be interacted with
  // via trusted user gestures.
  const newFileHandle = new FakeFileSystemFileHandle();
  const chooseEntries = new Promise(resolve => {
    window.chooseFileSystemEntries = options => {
      resolve(options);
      return newFileHandle;
    };
  });
  const testImage = await createTestImageFile(10, 10);
  await loadFile(testImage, new FakeFileSystemFileHandle());

  const result = await guestMessagePipe.sendMessage('test', {saveCopy: true});
  assertEquals(result.testQueryResult, 'boo yah!');
  const options = await chooseEntries;

  assertEquals(options.type, 'save-file');
  assertEquals(options.accepts.length, 1);
  assertEquals(options.accepts[0].extension, 'png');
  assertEquals(options.accepts[0].mimeTypes.length, 1);
  assertEquals(options.accepts[0].mimeTypes[0], 'image/png');

  const writeResult = await newFileHandle.lastWritable.closePromise;
  assertEquals(await writeResult.text(), await testImage.text());
  testDone();
});

TEST_F('MediaAppUIBrowserTest', 'RelatedFiles', async () => {
  const testFiles = [
    {name: 'matroska.mkv'},
    {name: 'jaypeg.jpg', type: 'image/jpeg'},
    {name: 'text.txt', type: 'text/plain'},
    {name: 'jiff.gif', type: 'image/gif'},
    {name: 'world.webm', type: 'video/webm'},
    {name: 'other.txt', type: 'text/plain'},
    {name: 'noext', type: ''},
    {name: 'html', type: 'text/html'},
    {name: 'matroska.emkv'},
  ];
  const directory = await createMockTestDirectory(testFiles);
  const [mkv, jpg, txt, gif, webm, other, ext, html] = directory.getFilesSync();
  const imageAndVideoFiles = [mkv, jpg, gif, webm];

  // Checks that the `currentFiles` array maintained by launch.js has everything
  // in `expectedFiles`, and nothing extra.
  function assertFilesToBe(testCase, expectedFiles) {
    // If lengths match, we can just check that all `expectedFiles` are present.
    assertEquals(
        expectedFiles.length, currentFiles.length,
        `Unexpected currentFiles length for ${testCase}`);
    for (const f of expectedFiles) {
      assertTrue(
          !!currentFiles.find(descriptor => descriptor.file.name === f.name),
          `${f.name} missing for ${testCase}`);
    }
  }

  await setCurrentDirectory(directory, mkv);
  assertFilesToBe('mkv', imageAndVideoFiles);

  await setCurrentDirectory(directory, jpg);
  assertFilesToBe('jpg', imageAndVideoFiles);

  await setCurrentDirectory(directory, gif);
  assertFilesToBe('gif', imageAndVideoFiles);

  await setCurrentDirectory(directory, webm);
  assertFilesToBe('webm', imageAndVideoFiles);

  await setCurrentDirectory(directory, txt);
  assertFilesToBe('txt', [txt, other]);

  await setCurrentDirectory(directory, html);
  assertFilesToBe('html', [html]);

  await setCurrentDirectory(directory, ext);
  assertFilesToBe('ext', [ext]);

  testDone();
});

// Test cases injected into the guest context.
// See implementations in media_app_guest_ui_browsertest.js.

TEST_F('MediaAppUIBrowserTest', 'GuestCanSpawnWorkers', async () => {
  await runTestInGuest('GuestCanSpawnWorkers');
  testDone();
});

TEST_F('MediaAppUIBrowserTest', 'GuestHasLang', async () => {
  await runTestInGuest('GuestHasLang');
  testDone();
});

TEST_F('MediaAppUIBrowserTest', 'GuestCanLoadWithCspRestrictions', async () => {
  await runTestInGuest('GuestCanLoadWithCspRestrictions');
  testDone();
});

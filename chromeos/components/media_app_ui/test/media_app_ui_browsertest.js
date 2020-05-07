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

let driver = null;

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
 * @return {Promise<File>} A {width}x{height} transparent encoded image/png.
 */
async function createTestImageFile(
    width = TEST_IMAGE_WIDTH, height = TEST_IMAGE_HEIGHT) {
  const canvas = new OffscreenCanvas(width, height);
  canvas.getContext('2d');  // convertToBlob fails without a rendering context.
  const blob = await canvas.convertToBlob();
  return new File([blob], 'test_file.png', {type: 'image/png'});
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

TEST_F('MediaAppUIBrowserTest', 'LoadFile', async () => {
  await loadFile(await createTestImageFile(), new FakeFileSystemFileHandle());
  const result =
      await driver.waitForElementInGuest('img[src^="blob:"]', 'naturalWidth');

  assertEquals(`${TEST_IMAGE_WIDTH}`, result);
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
  let errorMessage = '';

  try {
    await guestMessagePipe.sendMessage('unknown-message', null);
  } catch (error) {
    errorMessage = error.message;
  }

  assertEquals(
      errorMessage,
      'No handler registered for message type \'unknown-message\'');
  testDone();
});

// Tests that we receive an error if the handler fails.
TEST_F('MediaAppUIBrowserTest', 'ReceivesProxiedError', async () => {
  guestMessagePipe.logClientError = error => console.log(JSON.stringify(error));
  let errorMessage = '';

  try {
    await guestMessagePipe.sendMessage('bad-handler', null);
  } catch (error) {
    errorMessage = error.message;
  }

  assertEquals(errorMessage, 'This is an error');
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

// Tests the IPC behind the implementation of ReceivedFile.deleteOriginalFile()
// in the untrusted context.
TEST_F('MediaAppUIBrowserTest', 'DeleteOriginalIPC', async () => {
  const directory = await createMockTestDirectory();
  // Simulate steps taken to load a file via a launch event.
  const firstFile = directory.files[0];
  await loadFile(await createTestImageFile(), firstFile);
  // Set `currentDirectoryHandle` in launch.js.
  currentDirectoryHandle = directory;
  let testResponse;

  // Nothing should be deleted initially.
  assertEquals(null, directory.lastDeleted);

  const messageDelete = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDelete);

  // Assertion will fail if exceptions from launch.js are thrown, no exceptions
  // indicates the file was successfully deleted.
  assertEquals(
      testResponse.testQueryResult, 'deleteOriginalFile resolved success');
  assertEquals(firstFile, directory.lastDeleted);
  // File removed from `DirectoryHandle` internal state.
  assertEquals(directory.files.length, 0);

  // Even though the name is the same, the new file shouldn't
  // be deleted as it has a different `FileHandle` reference.
  directory.addFileHandleForTest(new FakeFileSystemFileHandle());

  // Try delete the first file again, should result in file moved.
  const messageDeleteMoved = {deleteLastFile: true};
  testResponse = await guestMessagePipe.sendMessage('test', messageDeleteMoved);

  assertEquals(
      testResponse.testQueryResult, 'deleteOriginalFile resolved file moved');
  // New file not removed from `DirectoryHandle` internal state.
  assertEquals(directory.files.length, 1);
  testDone();
});

// Tests the IPC behind the loadNext and loadPrev functions on the received file
// list in the untrusted context.
TEST_F('MediaAppUIBrowserTest', 'NavigateIPC', async () => {
  await loadMultipleFiles([
    {file: await createTestImageFile(), handle: new FakeFileSystemFileHandle()},
    {file: await createTestImageFile(), handle: new FakeFileSystemFileHandle()}
  ]);
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

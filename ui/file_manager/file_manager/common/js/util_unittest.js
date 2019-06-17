// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileSystem;

function setUp() {
  // Override VolumeInfo.prototype.resolveDisplayRoot to be sync.
  VolumeInfoImpl.prototype.resolveDisplayRoot = function(successCallback) {
    this.displayRoot_ = this.fileSystem_.root;
    successCallback(this.displayRoot_);
    return Promise.resolve(this.displayRoot_);
  };

  fileSystem = new MockFileSystem('fake-volume');
  const filenames = [
    '/file_a.txt',
    '/file_b.txt',
    '/file_c.txt',
    '/file_d.txt',
    '/dir_a/file_e.txt',
    '/dir_a/file_f.txt',
    '/dir_a/dir_b/dir_c/file_g.txt',
  ];
  fileSystem.populate(filenames);

  window.loadTimeData.getString = id => id;
}

function testReadEntriesRecursively(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If all directories are read recursively, found files should be 6.
        assertEquals(7, foundEntries.length);
        callback();
      },
      () => {}, () => false);
}

function testReadEntriesRecursivelyLevel0(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If only the top directory is read, found entries should be 3.
        assertEquals(4, foundEntries.length);
        callback();
      },
      () => {}, () => false, 0 /* opt_maxDepth */);
}

function testReadEntriesRecursivelyLevel1(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If we delve directories only one level, found entries should be 5.
        assertEquals(6, foundEntries.length);
        callback();
      },
      () => {}, () => false, 1 /* opt_maxDepth */);
}


function testIsDescendantEntry() {
  const root = fileSystem.root;
  const folder = fileSystem.entries['/dir_a'];
  const subFolder = fileSystem.entries['/dir_a/dir_b'];
  const file = fileSystem.entries['/file_a.txt'];
  const deepFile = fileSystem.entries['/dir_a/dir_b/dir_c/file_g.txt'];

  const fakeEntry =
      new FakeEntry('fake-entry-label', VolumeManagerCommon.RootType.CROSTINI);

  const entryList =
      new EntryList('entry-list-label', VolumeManagerCommon.RootType.MY_FILES);
  entryList.addEntry(fakeEntry);

  const volumeManager = new MockVolumeManager();
  // Index 1 is Downloads.
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);
  const downloadsVolumeInfo = volumeManager.volumeInfoList.item(1);
  const mockFs = /** @type {MockFileSystem} */ (downloadsVolumeInfo.fileSystem);
  mockFs.populate(['/folder1/']);
  const folder1 = downloadsVolumeInfo.fileSystem.entries['/folder1'];

  const volumeEntry = new VolumeEntry(downloadsVolumeInfo);
  volumeEntry.addEntry(fakeEntry);

  // No descendants.
  assertFalse(util.isDescendantEntry(file, file));
  assertFalse(util.isDescendantEntry(root, root));
  assertFalse(util.isDescendantEntry(deepFile, root));
  assertFalse(util.isDescendantEntry(subFolder, root));
  assertFalse(util.isDescendantEntry(fakeEntry, root));
  assertFalse(util.isDescendantEntry(root, fakeEntry));
  assertFalse(util.isDescendantEntry(fakeEntry, entryList));
  assertFalse(util.isDescendantEntry(fakeEntry, volumeEntry));
  assertFalse(util.isDescendantEntry(folder1, volumeEntry));

  assertTrue(util.isDescendantEntry(root, file));
  assertTrue(util.isDescendantEntry(root, subFolder));
  assertTrue(util.isDescendantEntry(root, deepFile));
  assertTrue(util.isDescendantEntry(root, folder));
  assertTrue(util.isDescendantEntry(folder, subFolder));
  assertTrue(util.isDescendantEntry(folder, deepFile));
  assertTrue(util.isDescendantEntry(entryList, fakeEntry));
  assertTrue(util.isDescendantEntry(volumeEntry, fakeEntry));
  assertTrue(util.isDescendantEntry(volumeEntry, folder1));
}

/**
 * Tests that it doesn't fail with different types of entries and inputs.
 */
function testEntryDebugString() {
  // Check static values.
  assertEquals('entry is null', util.entryDebugString(null));
  (/**
    * @suppress {checkTypes} Closure doesn't allow passing undefined or {} due
    * to type constraints nor casting to {Entry}.
    */
   function() {
     assertEquals('entry is undefined', util.entryDebugString(undefined));
     assertEquals('(Object) ', util.entryDebugString({}));
   })();

  // Construct some types of entries.
  const root = fileSystem.root;
  const folder = fileSystem.entries['/dir_a'];
  const file = fileSystem.entries['/file_a.txt'];
  const fakeEntry =
      new FakeEntry('fake-entry-label', VolumeManagerCommon.RootType.CROSTINI);
  const entryList =
      new EntryList('entry-list-label', VolumeManagerCommon.RootType.MY_FILES);
  entryList.addEntry(fakeEntry);
  const volumeManager = new MockVolumeManager();
  // Index 1 is Downloads.
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);
  const downloadsVolumeInfo = volumeManager.volumeInfoList.item(1);
  const mockFs = /** @type {MockFileSystem} */ (downloadsVolumeInfo.fileSystem);
  mockFs.populate(['/folder1/']);
  const volumeEntry = new VolumeEntry(downloadsVolumeInfo);
  volumeEntry.addEntry(fakeEntry);

  // Mocked values are identified as Object instead of DirectoryEntry and
  // FileEntry.
  assertEquals(
      '(Object) / filesystem:fake-volume/', util.entryDebugString(root));
  assertEquals(
      '(Object) /dir_a filesystem:fake-volume/dir_a',
      util.entryDebugString(folder));
  assertEquals(
      '(Object) /file_a.txt filesystem:fake-volume/file_a.txt',
      util.entryDebugString(file));
  // FilesAppEntry types:
  assertEquals(
      '(FakeEntry) / fake-entry://crostini', util.entryDebugString(fakeEntry));
  assertEquals(
      '(EntryList) / entry-list://my_files', util.entryDebugString(entryList));
  assertEquals(
      '(VolumeEntry) / filesystem:downloads/',
      util.entryDebugString(volumeEntry));
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Reply to test messages. Contents depend on the test message sent.
 * @typedef {{
 *     testQueryResult: string,
 *     testQueryResultData: (!Object|undefined)
 * }}
 */
let TestMessageResponseData;

/**
 * Object sent over postMessage to run a command or extract data.
 * @typedef {{
 *     deleteLastFile: (boolean|undefined),
 *     getFileErrors: (boolean|undefined),
 *     navigate: (string|undefined),
 *     overwriteLastFile: (string|undefined),
 *     pathToRoot: (!Array<string>|undefined),
 *     property: (string|undefined),
 *     renameLastFile: (string|undefined),
 *     requestFullscreen: (boolean|undefined),
 *     requestSaveFile: (boolean|undefined),
 *     saveCopy: (boolean|undefined),
 *     testQuery: string,
 * }}
 */
let TestMessageQueryData;

/** @typedef {{testCase: string}} */
let TestMessageRunTestCase;

/**
 * Return type of `get-last-loaded-files` used to spy on the files sent to the
 * guest app using `loadFiles()`. We pass `ReceivedFileList.files` since passing
 * `ReceivedFileList` through different contexts prunes methods and fails due to
 * observers.
 * @typedef {{fileList: ?Array<!mediaApp.AbstractFile>}}
 */
let LastLoadedFilesResponse;

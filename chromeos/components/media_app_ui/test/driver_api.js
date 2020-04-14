// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{testQueryResult: string}} */
let TestMessageResponseData;

/**
 * @typedef {{
 *     deleteLastFile: (boolean|undefined),
 *     navigate: (string|undefined),
 *     overwriteLastFile: (string|undefined),
 *     pathToRoot: (Array<string>|undefined),
 *     property: (string|undefined),
 *     renameLastFile: (string|undefined),
 *     requestFullscreen: (boolean|undefined),
 *     saveCopy: (boolean|undefined),
 *     testQuery: string,
 * }}
 */
let TestMessageQueryData;

/** @typedef {{testCase: string}} */
let TestMessageRunTestCase;

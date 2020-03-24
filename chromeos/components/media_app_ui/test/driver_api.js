// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{testQueryResult: string}} */
var TestMessageResponseData;

/**
 * @typedef {{
 *     testQuery: string,
 *     pathToRoot: (Array<string>|undefined),
 *     property: (string|undefined),
 *     requestFullscreen: (boolean|undefined),
 *     overwriteLastFile: (string|undefined),
 *     deleteLastFile: (boolean|undefined)
 * }}
 */
var TestMessageQueryData;

/** @type {MessagePipe} */
var guestMessagePipe;

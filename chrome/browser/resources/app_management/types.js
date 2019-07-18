// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for App Management.
 */

/**
 * @typedef {appManagement.mojom.App}
 */
let App;

/**
 * @typedef {appManagement.mojom.ExtensionAppPermissionMessage}
 */
let ExtensionAppPermissionMessage;

/**
 * @typedef {apps.mojom.Permission}
 */
let Permission;

/**
 * Maps app ids to Apps.
 * @typedef {!Object<string, App>}
 */
let AppMap;

/**
 * @typedef {{
 *   pageType: PageType,
 *   selectedAppId: ?string,
 * }}
 */
let Page;

/**
 * @typedef {{
 *   term: ?string,
 *   results: ?Array<App>,
 * }}
 */
let SearchState;

/**
 * @typedef {{
 *   allowedIds: !Set<string>,
 *   blockedIds: !Set<string>,
 * }}
 */
let NotificationsState;

/**
 * @typedef {{
 *   apps: !AppMap,
 *   currentPage: !Page,
 *   arcSupported: boolean,
 *   search: !SearchState,
 *   notifications: !NotificationsState,
 * }}
 */
let AppManagementPageState;

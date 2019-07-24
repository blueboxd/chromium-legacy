// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Create an app for testing purpose.
 * @param {string} id
 * @param {Object=} optConfig
 * @return {!App}
 */
function createApp(id, config) {
  return app_management.FakePageHandler.createApp(id, config);
}

/**
 * @return {app_management.FakePageHandler}
 */
function setupFakeHandler() {
  const browserProxy = app_management.BrowserProxy.getInstance();
  const fakeHandler = new app_management.FakePageHandler(
      browserProxy.callbackRouter.$.bindNewPipeAndPassRemote());
  browserProxy.handler = fakeHandler.getRemote();

  return fakeHandler;
}

/**
 * Replace the app management store instance with a new, empty TestStore.
 * @return {app_management.TestStore}
 */
function replaceStore() {
  let store = new app_management.TestStore();
  store.setReducersEnabled(true);
  store.replaceSingleton();
  return store;
}

/**
 * @param {Element} element
 * @return {bool}
 */
function isHidden(element) {
  const rect = element.getBoundingClientRect();
  return rect.height === 0 && rect.width === 0;
}

/**
 * Replace the current body of the test with a new element.
 * @param {Element} element
 */
function replaceBody(element) {
  PolymerTest.clearBody();

  window.history.replaceState({}, '', '/');

  document.body.appendChild(element);
}

/** @return {String} */
function getCurrentUrlSuffix() {
  return window.location.href.slice(window.location.origin.length);
}

/** @param {String} route  */
async function navigateTo(route) {
  window.history.replaceState({}, '', route);
  window.dispatchEvent(new CustomEvent('location-changed'));
  await PolymerTest.flushTasks();
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
function getPermissionItemByType(view, permissionType) {
  return view.root.querySelector('[permission-type=' + permissionType + ']');
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
function getPermissionToggleByType(view, permissionType) {
  return getPermissionItemByType(view, permissionType)
      .root.querySelector('app-management-permission-toggle');
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
function getPermissionCrToggleByType(view, permissionType) {
  return getPermissionToggleByType(view, permissionType)
      .root.querySelector('cr-toggle');
}

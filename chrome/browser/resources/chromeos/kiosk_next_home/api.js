// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Chrome OS Kiosk Next Home API definition.
 */

/**
 * Namespace for the Kiosk Next Home bridge and related data.
 * @const
 */
var kioskNextHome = {};

/**
 * System bridge API for the Kiosk Next Home.
 *
 * @interface
 */
kioskNextHome.Bridge = class {
  /**
   * Adds listener for system events.
   * @param {!kioskNextHome.Listener} listener Listener for system events.
   * @deprecated Use setListener instead.
   * TODO(ltenorio): Remove usage and remove this method.
   */
  addListener(listener) {}

  /**
   * Sets the current listener for system events. If null, we will no longer
   * send events to the previous listener.
   *
   * @param {?kioskNextHome.Listener} listener Listener for system events.
   */
  setListener(listener) {}

  /**
   * @return {!Promise<string>} Promise for the user's given name.
   */
  getUserGivenName() {}

  /**
   * @return {!Promise<string>} Promise for the user's display name.
   */
  getUserDisplayName() {}

  /**
   * Gets the obfuscated account Gaia ID associated with the current user
   * session.
   * @return {!Promise<string>} Promise for the obfuscated account Gaia ID.
   */
  getAccountId() {}

  /**
   * Returns an access token with the requested scopes.
   * @param {!Array<string>} scopes List of scopes to use when obtaining access
   *     token.
   * @return {!Promise<string>} Promise for the access token.
   * @deprecated Use fetchAccessToken instead. This version doesn't return the
   *    token expiration.
   */
  getAccessToken(scopes) {}

  /**
   * Returns an access token with the requested scopes.
   * @param {!Array<string>} scopes List of scopes to use when obtaining access
   *     token.
   * @return {!Promise<kioskNextHome.AccessToken>} Promise for the access
   *     token.
   */
  fetchAccessToken(scopes) {}

  /**
   * Returns the Android ID for the ARC++ container. This call might fail if the
   * ARC provisioning is not complete (i.e. user has not finished the ARC setup
   * flow).
   * @return {!Promise<string>} Promise for the Android ID.
   */
  getAndroidId() {}

  /**
   * Returns a list of apps installed in the user session.
   * @return {!Promise<!Array<!kioskNextHome.App>>} Promise for the list of
   *     apps.
   */
  getApps() {}

  /**
   * Launches the app with the given appId.
   *
   * Note: This is a fire and forget operation, given the VMs involved and
   * their IPC interfaces, we can't tell if the operation was successful.
   * @param {string} appId Chrome OS identifier for the app.
   */
  launchApp(appId) {}

  /**
   * Launches an allowed ARC intent. Intents are checked via
   * chromeos::kiosk_next_home::IntentConfigHelper.
   * @param {string} intent
   * @return {!Promise} Resolves if intent was allowed and launched; rejects
   *     otherwise.
   */
  launchIntent(intent) {}

  /**
   * Opens the website in the customized KioskNext Browser.
   * @param {!url.mojom.Url} url
   */
  launchKioskNextWebsite(url) {}

  /**
   * Opens the website in the tabbed browser.
   * @param {!url.mojom.Url} url
   */
  launchWebsite(url) {}

  /**
   * Shows a prompt to uninstall the app with the given appId.
   *
   * We will notify listeners when the app is uninstalled through a readiness
   * state change.
   * @param {string} appId App to uninstall.
   */
  uninstallApp(appId) {}

  /**
   * Returns current device network state.
   * @return {kioskNextHome.NetworkState}
   */
  getNetworkState() {}
};

/**
 * A record representing an access token.
 * @record
 */
kioskNextHome.AccessToken = class {
  constructor() {
    /** @type {string} The access token. */
    this.token;
    /**
     * @type {number} Time when this token will expire in milliseconds from the
     * Unix epoch.
     */
    this.expirationTime;
  }
};

/**
 * Types of installed apps on Chrome OS.
 * @enum {string}
 */
kioskNextHome.AppType = {
  /**
   * The app type could not be determined or is not relevant to Kiosk Next (e.g.
   * Linux apps).
   */
  UNKNOWN: 'unknown',
  /** The app is an ARC++ app (Android app). */
  ARC: 'arc',
  /**
   * The app is a Chrome OS app.
   *
   * Note: This is an artificial grouping of the many different types of apps
   * that Chrome OS supports. Built-in apps (e.g. Settings), Extensions (e.g.
   * Files app) and pinned web apps. They mostly behave in the same way from
   * the point of view of Kiosk Next.
   */
  CHROME: 'chrome',
};

/**
 * Readiness status for apps.
 * These values loosely map to AppService's apps.mojom.Readiness enum.
 * @enum {string}
 */
kioskNextHome.AppReadiness = {
  /**
   * App readiness could not be determined.
   * This can happen if the current app is unavailable for unknown or
   * unexpected reasons, like a renderer crash for Extensions or an unsupported
   * operation (such as blacklisted web apps).
   */
  UNKNOWN: 'unknown',
  /** Installed and launchable. */
  READY: 'ready',
  /** App is disabled by policy. */
  DISABLED: 'disabled',
  /** App was uninstalled by user. */
  UNINSTALLED: 'uninstalled',
};

/**
 * A record representing an installed app on the system.
 * @record
 */
kioskNextHome.App = class {
  constructor() {
    /** @type {string} Unique Chrome OS identifier for the app. */
    this.appId;
    /** @type {kioskNextHome.AppType} */
    this.type;
    /**
     * @type {string | undefined} Android package name, if it's an ARC++ app.
     */
    this.packageName;
    /** @type {string} Readable name to display. */
    this.displayName;
    /** @type {string | undefined} Base64-encoded thumbnail image, fallback. */
    this.thumbnailImage;
    /**
     * @type {kioskNextHome.AppReadiness} Current readiness state for the app.
     */
    this.readiness;
  }
};

/**
 * Current network state of the device.
 * @enum {string}
 */
kioskNextHome.NetworkState = {
  ONLINE: 'online',
  OFFLINE: 'offline',
};

/**
 * Current status for ARC instance.
 * @enum {string}
 */
kioskNextHome.ArcStatus = {
  STOPPED: 'stopped',
  READY: 'ready',
};

/**
 * Interface for a listener of system events, subscribed via
 * {!kioskNextHome.Bridge}.
 *
 * @interface
 */
kioskNextHome.Listener = class {
  /**
   * Called when an app state changes.
   * @param {!kioskNextHome.App} app The app whose state changed.
   */
  onAppChanged(app) {}

  /**
   * Called when the ARC status changes. Called at first when the listener is
   * set.
   * @param {!kioskNextHome.ArcStatus} status The new ARC status.
   */
  onArcStatusChanged(status) {}

  /**
   * Called when the network state changes.
   * @param {kioskNextHome.NetworkState} networkState Current network state of
   *     the device.
   */
  onNetworkStateChanged(networkState) {}
};

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with Chrome OS.
 */
kioskNextHome.getChromeOsBridge = function() {};

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Add functionality used in tests.
 */
goog.provide('__crWeb.cookieTest');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.cookieTest = {};

// Store namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['cookieTest'] = __gCrWeb.cookieTest;

__gCrWeb.cookieTest.getCookies = function() {
  return document.cookie;
};

__gCrWeb.cookieTest.setCookie = function(newCookie) {
  document.cookie = newCookie;
};

// Returns the localStorage entry for key. If there is an DOMException,
// returns a dictionary { message: message }.
__gCrWeb.cookieTest.getLocalStorage = function(key) {
  try {
    if (localStorage.getItem(key) == null) {
      return 'a';
    }
    return localStorage.getItem(key);
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return null;
  }
};

__gCrWeb.cookieTest.setLocalStorage = function(key, value) {
  try {
    localStorage.setItem(key, value);
    return true;
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return {'message': error.message};
  }
};

// Returns the sessionStorage entry for key. If there is an DOMException,
// returns a dictionary { message: message }.
__gCrWeb.cookieTest.getSessionStorage = function(key) {
  try {
    return sessionStorage.getItem(key);
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return null;
  }
};

__gCrWeb.cookieTest.setSessionStorage = function(key, value) {
  try {
    sessionStorage.setItem(key, value);
    return true;
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return false;
  }
};

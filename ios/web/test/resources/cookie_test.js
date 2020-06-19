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

async function setCache(key, value) {
  const cache = await caches.open('cache');
  return cache.put(`/${key}`, new Response(value));
}

async function getCache(key) {
  const cache = await caches.open('cache');
  const result = await cache.match(new Request(`/${key}`));
  return result && result.text();
}

function onError(error) {
  if (error instanceof DOMException || error instanceof ReferenceError) {
    __gCrWeb.message.invokeOnHost({
      command: 'cookieTest.result',
      result: {message: error.message}
    });
    return;
  }
  __gCrWeb.message.invokeOnHost(
      {command: 'cookieTest.result', result: false});
}

async function getCacheWrapper(key) {
  try {
    const value = await getCache(key);
    __gCrWeb.message.invokeOnHost(
        {command: 'cookieTest.result', result: value})
  } catch (error) {
    onError(error);
  }
}

async function setCacheWrapper(key, value) {
  try {
    await setCache(key, value);
    __gCrWeb.message.invokeOnHost(
        {command: 'cookieTest.result', result: true})
  } catch (error) {
    onError(error);
  }
}

__gCrWeb.cookieTest.setCache = function(key, value) {
  setCacheWrapper(key, value);
  return true;
};

__gCrWeb.cookieTest.getCache = function(key) {
  getCacheWrapper(key);
  return 'This is an async function.';
};

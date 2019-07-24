// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

suite('PromiseResolverModuleTest', function() {
  test('members read only', function() {
    const resolver = new PromiseResolver;
    assertThrows(function() {
      resolver.promise = new Promise;
    });
    assertThrows(function() {
      resolver.resolve = function() {};
    });
    assertThrows(function() {
      resolver.reject = function() {};
    });
  });

  test('resolves', function(done) {
    const resolver = new PromiseResolver;
    resolver.promise.then(done);
    resolver.resolve();
  });

  test('rejects', function(done) {
    const resolver = new PromiseResolver;
    resolver.promise.catch(done);
    resolver.reject();
  });

  test('is fulfilled', function() {
    const resolver1 = new PromiseResolver;
    assertFalse(resolver1.isFulfilled);
    resolver1.resolve();
    assertTrue(resolver1.isFulfilled);

    const resolver2 = new PromiseResolver;
    assertFalse(resolver2.isFulfilled);
    resolver2.resolve(true);
    assertTrue(resolver2.isFulfilled);

    const resolver3 = new PromiseResolver;
    assertFalse(resolver3.isFulfilled);
    resolver3.reject(new Error);
    assertTrue(resolver3.isFulfilled);
  });
});

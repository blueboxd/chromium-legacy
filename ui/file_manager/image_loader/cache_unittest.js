// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function testCreateCacheKey() {
  const key = LoadImageRequest.cacheKey({url: 'http://example.com/image.jpg'});
  assertTrue(!!key);
}

function testNotCreateCacheKey() {
  let key = LoadImageRequest.cacheKey({url: 'data:xxx'});
  assertFalse(!!key);

  key = LoadImageRequest.cacheKey({url: 'DaTa:xxx'});
  assertFalse(!!key);
}

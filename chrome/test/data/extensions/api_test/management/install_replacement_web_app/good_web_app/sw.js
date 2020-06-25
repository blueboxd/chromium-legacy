// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fetch handler that returns a response object when a network error happens.
self.addEventListener('fetch', e => {
  e.respondWith(
    fetch(e.request).catch(() => {
      return new Response('Hello offline page');
    })
  );
});

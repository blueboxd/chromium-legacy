// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sleep(delay) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve();
    }, delay);
  });
}

self.addEventListener("fetch", event => {
  const param = new URL(event.request.url).searchParams;

  if (param.has("fetch")) {
    event.respondWith(fetch(event.request.url));
  } else if (param.has("offline")) {
    event.respondWith(new Response("Hello Offline page"));
  } else if (param.has("fetch_or_offline")) {
    event.respondWith(
      fetch(event.request).catch(error => {
        return new Response("Hello Offline page");
      })
    );
  } else if (param.has("sleep_then_fetch")) {
    event.respondWith(
      sleep(param.get("sleep") || 0, event.request.url).then(() => {
        return fetch(event.request.url);
      })
    );
  } else if (param.has("sleep_then_offline")) {
    event.respondWith(
      sleep(param.get("sleep") || 0, event.request.url).then(() => {
        return new Response("Hello Offline page");
      })
    );
  } else {
    // fallback case: do nothing.
  }
});

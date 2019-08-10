// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CACHE_NAME = 'cache_name';
const SCRIPT_PATH = 'code_cache_strategy_test_script.js';

async function get_from_cache_or_fetch(request) {
  const cache = await caches.open(CACHE_NAME);
  const cached = await cache.match(request);
  if (cached) {
    return cached;
  }
  const response = await fetch(request);
  await cache.put(request, response.clone());
  return response;
}

async function cache_script_in_message_event(source) {
  const cache = await caches.open(CACHE_NAME);
  await cache.add(`${SCRIPT_PATH}?cached_in_message_event`);
  source.postMessage('DONE');
}

async function cache_script_twice(source) {
  const cache = await caches.open(CACHE_NAME);
  const url = `${SCRIPT_PATH}?cached_twice`;
  const response = await fetch(url);
  const dummy_response = new Response('/* dummy script */', {
    headers: {
      'Content-Type': 'application/javascript'
    }
  });

  // 1st step: put synthesized response without await.
  cache.put(url, dummy_response);
  // 2nd step: put the actual response.
  await cache.put(url, response);
  source.postMessage('DONE');
}

self.addEventListener('install', e => {
  e.waitUntil(async function() {
    const cache = await caches.open(CACHE_NAME);
    await cache.addAll([
      `${SCRIPT_PATH}?cached_in_install_event`,
    ]);

    // Create a synthetic response with a mime-type that has a charset
    const script_body = "function test_function() { return 'SUCCESS'; }";
    const response = new Response(script_body, {
      headers: { 'content-type': 'application/javascript; charset=utf-8' }
    });
    await cache.put(
        `${SCRIPT_PATH}?cached_in_install_event_with_mime_type_param`,
        response);
  }());
});

self.addEventListener('fetch', e => {
  const url = e.request.url;
  if (url.indexOf('cached_in_fetch_event') >= 0) {
    e.respondWith(get_from_cache_or_fetch(e.request));
  }
});

self.addEventListener('message', e => {
  const command = e.data.command;
  if (command === 'cache_script_in_message_event') {
    cache_script_in_message_event(e.source);
  } else if (command === 'cache_script_twice') {
    cache_script_twice(e.source);
  }
});

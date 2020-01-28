'use strict';

importScripts('/resources/testharness.js');

test(t => {
  assert_false('setExperimentalAppBadge' in navigator, 'setExperimentalAppBadge does not exist in navigator');
  assert_false('clearExperimentalAppBadge' in navigator, 'clearExperimentalAppBadge does not exist in  navigator');
}, 'Badge API interfaces and properties in Origin-Trial disabled service worker.');

done();

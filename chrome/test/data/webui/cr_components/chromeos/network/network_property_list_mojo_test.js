// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_property_list_mojo.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkPropertyListMojoTest', function() {
  /** @type {!NetworkPropertyListMojo|undefined} */
  let propertyList;

  setup(function() {
    propertyList = document.createElement('network-property-list-mojo');
    const ipv4 = {
      ipAddress: "100.0.0.1",
      type: "IPv4",
    };
    propertyList.propertyDict = {ipv4: ipv4};
    propertyList.fields = [
      'ipv4.ipAddress',
      'ipv4.routingPrefix',
      'ipv4.gateway',
      'ipv6.ipAddress',
    ];

    document.body.appendChild(propertyList);
    Polymer.dom.flush();
  });

  test('Editable field types', async () => {
    // ipv4.ipAddress is not set as editable (via |editFieldTypes|), so the
    // edit input does not exist.
    assertEquals(null, propertyList.$$('cr-input'));

    // Set ipv4.ipAddress field as editable.
    propertyList.editFieldTypes = {
      'ipv4.ipAddress': 'String',
    };
    Polymer.dom.flush();

    // The input to edit the property now exists.
    assertNotEquals(null, propertyList.$$('cr-input'));
  });
});

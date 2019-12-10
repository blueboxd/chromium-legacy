// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Tests curl command generation\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  var logView = UI.panels.network._networkLogView;

  function newRequest(isBlob, headers, data, opt_url) {
    var request = new SDK.NetworkRequest(0, (isBlob === true ? 'blob:' : '') + (opt_url || 'http://example.org/path'), 0, 0, 0);
    request.requestMethod = data ? 'POST' : 'GET';
    var headerList = [];
    if (headers) {
      for (var i in headers)
        headerList.push({name: i, value: headers[i]});
    }
    request.setRequestFormData(!!data, data);
    if (data)
      headerList.push({name: 'Content-Length', value: data.length});
    request.setRequestHeaders(headerList);
    return request;
  }

  async function dumpRequest(headers, data, opt_url) {
    var curlWin = await logView._generateCurlCommand(newRequest(false, headers, data, opt_url), 'win');
    var curlUnix = await logView._generateCurlCommand(newRequest(false, headers, data, opt_url), 'unix');
    var powershell = await logView._generatePowerShellCommand(newRequest(false, headers, data, opt_url));
    var fetch = await logView._generateFetchCall(newRequest(false, headers, data, opt_url));
    TestRunner.addResult(`cURL Windows: ${curlWin}`);
    TestRunner.addResult(`cURL Unix: ${curlUnix}`);
    TestRunner.addResult(`Powershell: ${powershell}`);
    TestRunner.addResult(`fetch: ${fetch}`);
  }

  async function dumpMultipleRequests(blobPattern) {
    const header = {'Content-Type': 'foo/bar'};
    const data = 'baz';
    const allRequests = blobPattern.map(isBlob => newRequest(isBlob, header, data));

    var allCurlWin = await logView._generateAllCurlCommand(allRequests, 'win');
    var allCurlUnix = await logView._generateAllCurlCommand(allRequests, 'unix');
    var allPowershell = await logView._generateAllPowerShellCommand(allRequests);
    var allFetch = await logView._generateAllFetchCall(allRequests);
    TestRunner.addResult(`cURL Windows: ${allCurlWin}`);
    TestRunner.addResult(`cURL Unix: ${allCurlUnix}`);
    TestRunner.addResult(`Powershell: ${allPowershell}`);
    TestRunner.addResult(`fetch: ${allFetch}`);
  }

  await dumpRequest({});
  await dumpRequest({}, '123');
  await dumpRequest({'Content-Type': 'application/x-www-form-urlencoded'}, '1&b');
  await dumpRequest({'Content-Type': 'application/json'}, '{"a":1}');
  await dumpRequest(
      {'Content-Type': 'application/binary'}, '1234\r\n00\x02\x03\x04\x05\'"!');
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '1234\r\n\x0100\x02\x03\x04\x05\'"!');
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '%OS%\r\n%%OS%%\r\n"\\"\'$&!');  // Ensure %OS% for windows is not env evaled
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '!@#$%^&*()_+~`1234567890-=[]{};\':",./\r<>?\r\nqwer\nt\n\nyuiopasdfghjklmnbvcxzQWERTYUIOPLKJHGFDSAZXCVBNM');
  await dumpRequest({'Content-Type': 'application/binary'}, '\x7F\x80\x90\xFF\u0009\u0700');
  await dumpRequest(
      {}, null,
      'http://labs.ft.com/?querystring=[]{}');  // Character range symbols must be escaped (http://crbug.com/265449).
  await dumpRequest({'Content-Type': 'application/binary'}, '%PATH%$PATH');
  await dumpRequest({':host': 'h', 'version': 'v'});
  await dumpRequest({'Cookie': '_x=fdsfs; aA=fdsfdsf; FOO=ID=BAR:BAZ=FOO:F=d:AO=21.212.2.212-:A=dsadas8d9as8d9a8sd9sa8d9a; AAA=117'});

  await dumpMultipleRequests([]);
  await dumpMultipleRequests([true]);
  await dumpMultipleRequests([true, true]);
  await dumpMultipleRequests([false]);
  await dumpMultipleRequests([true, false]);

  TestRunner.completeTest();
})();

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules': {
    'no-restricted-properties': [
      'error',
      {
        'object': 'MockInteractions',
        'property': 'tap',
        'message': 'Do not use on-tap handlers in prod code, and use the ' +
            'native click() method in tests. See more context at ' +
            'crbug.com/812035.',
      },
    ],
    'no-var': 'error',
    'prefer-const': 'error',
  },
};

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {generateUrlFromFilter} from '../url_processor.js';

const LinkToGraph = {
  props: {
    filter: Array,
    graphType: String,
    text: String,
  },
  computed: {
    url: function() {
      return generateUrlFromFilter(document.URL, this.graphType, this.filter);
    },
  },
  template: `<a :href="this.url">{{this.text}}</a>`,
};

export {
  LinkToGraph,
};

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {generateUrlFromFilter} from '../url_processor.js';

const PageUrlGenerator = {
  props: ['nodeFilterData'],
  data: function() {
    return this.nodeFilterData;
  },
  methods: {
    /**
     * Generates an URL for the current page containing the filter in its
     * querystring, then copies the URL to the input elem and highlights it.
     */
    generateUrl: function() {
      const pageUrl = generateUrlFromFilter(this.nodeList, document.URL);
      this.$refs.input.value = pageUrl;
      this.$refs.input.select();
    },
  },
  template: `
    <div class="url-generator">
      <button @click="generateUrl">Generate Current URL</button>
      <input type="text" readonly ref="input">
    </div>`,
};

export {
  PageUrlGenerator,
};

<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="url-generator">
    <button @click="generateUrl">
      Generate Current URL
    </button>
    <input
        ref="input"
        type="text"
        readonly>
  </div>
</template>

<script>
import {generateUrlFromFilter} from '../url_processor.js';

const PageUrlGenerator = {
  props: {
    pagePathName: String,
    nodeFilterData: Object,
  },
  data: function() {
    return this.nodeFilterData;
  },
  methods: {
    /**
     * Generates an URL for the current page containing the filter in its
     * querystring, then copies the URL to the input elem and highlights it.
     */
    generateUrl: function() {
      const pageUrl = generateUrlFromFilter(
          document.URL, this.pagePathName, this.nodeList);
      this.$refs.input.value = pageUrl;
      this.$refs.input.select();
    },
  },
};

export default PageUrlGenerator;
</script>

<style scoped>
.url-generator {
  display: flex;
  flex-direction: row;
}
</style>

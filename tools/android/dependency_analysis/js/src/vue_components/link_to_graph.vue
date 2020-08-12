<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <a
      class="link-to-graph-container"
      :href="url">
    <img
        class="link-to-graph-icon"
        :src="GraphIcon">
    <div class="link-to-graph-text">
      {{ text }}
    </div>
  </a>
</template>

<script>
import {UrlProcessor, URL_PARAM_KEYS} from '../url_processor.js';

import GraphIcon from '../assets/graph_icon.png';

// @vue/component
const LinkToGraph = {
  props: {
    filter: Array,
    graphType: String,
    text: String,
  },
  computed: {
    GraphIcon: () => GraphIcon,
    url: function() {
      const urlProcessor = UrlProcessor.createForOutput();
      urlProcessor.appendArray(URL_PARAM_KEYS.FILTER_NAMES, this.filter);
      return urlProcessor.getUrl(document.URL, this.graphType);
    },
  },
};

export default LinkToGraph;
</script>

<style scoped>
.link-to-graph-container {
  align-items: center;
  display: flex;
  flex-direction: row;
}

.link-to-graph-icon {
  height: 24px;
  margin-right: 10px;
  width: 24px;
}

.link-to-graph-text {
  min-width: 0;
  white-space: normal;
  word-wrap: break-word;
}
</style>

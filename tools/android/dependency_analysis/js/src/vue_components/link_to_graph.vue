<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <a :href="url">{{ text }}</a>
</template>

<script>
import {UrlProcessor, URL_PARAM_KEYS} from '../url_processor.js';

// @vue/component
const LinkToGraph = {
  props: {
    filter: Array,
    graphType: String,
    text: String,
  },
  computed: {
    url: function() {
      const urlProcessor = UrlProcessor.createForOutput();
      urlProcessor.appendArray(URL_PARAM_KEYS.FILTER_NAMES, this.filter);
      return urlProcessor.getUrl(document.URL, this.graphType);
    },
  },
};

export default LinkToGraph;
</script>

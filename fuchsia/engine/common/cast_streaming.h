// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_CAST_STREAMING_H_
#define FUCHSIA_ENGINE_COMMON_CAST_STREAMING_H_

class GURL;

// Returns true if Cast Streaming is enabled for this process.
bool IsCastStreamingEnabled();

// Returns true if |url| is the Cast Streaming media source URL.
bool IsCastStreamingMediaSourceUrl(const GURL& url);

#endif  // FUCHSIA_ENGINE_COMMON_CAST_STREAMING_H_

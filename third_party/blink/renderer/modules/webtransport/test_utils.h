// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for testing classes in this directory. They assume they are running
// inside a gtest test.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_TEST_UTILS_H_

#include "mojo/public/cpp/system/data_pipe.h"
#include "v8/include/v8.h"

namespace blink {

class V8TestingScope;
class ReadableStream;

// Creates a mojo data pipe with the normal options for WebTransportTests.
// Returns true for success. Returns false and causes a test failure
// otherwise.
bool CreateDataPipeForWebTransportTests(
    mojo::ScopedDataPipeProducerHandle* producer,
    mojo::ScopedDataPipeConsumerHandle* consumer);

// Read the next value from a stream. It is expected that there is a value,
// ie. that |done| will be false. Fails the test if no value is available.
v8::Local<v8::Value> ReadValueFromStream(const V8TestingScope& scope,
                                         ReadableStream* stream);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_TEST_UTILS_H_

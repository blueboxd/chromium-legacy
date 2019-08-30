// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_H_
#define FUCHSIA_ENGINE_COMMON_H_

#include <zircon/processargs.h>

#include "fuchsia/engine/web_engine_export.h"

// This file contains constants and functions shared between Context and
// ContextProvider processes.

// Switch passed to Context process when running in incognito mode, i.e. when
// there is no kWebContextDataPath.
WEB_ENGINE_EXPORT extern const char kIncognitoSwitch[];

// Switch passed to Context process when enabling remote debugging. It takes
// a comma-separated list of remote debugger handle IDs as an argument.
WEB_ENGINE_EXPORT extern const char kRemoteDebuggerHandles[];

// Switch passed to Context process to customize the UserAgent string.
WEB_ENGINE_EXPORT extern const char kUserAgentProductAndVersion[];

// Handle ID for the Context interface request passed from ContextProvider to
// Context process.
constexpr uint32_t kContextRequestHandleId = PA_HND(PA_USER0, 0);

// The URL scheme used to access content directories.
WEB_ENGINE_EXPORT extern const char
    kFuchsiaContentDirectoryScheme[];  // "fuchsia-dir"

// The command line switch used to register custom schemes with a process.
// The contents are structured as a list of key-value pairs which map the scheme
// name to the handle of a Fuchsia::io::Directory channel.
//     e.g. foo=1234,bar=5678
// The switch can be used to send names by themselves, by pairing the names with
// zeroed handles.
WEB_ENGINE_EXPORT extern const char
    kContentDirectories[];  // "content-directories";

#endif  // FUCHSIA_ENGINE_COMMON_H_

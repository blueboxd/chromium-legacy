// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GC_CORE_GC_EXPORT_H_
#define COMPONENTS_GC_CORE_GC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GC_IMPLEMENTATION)
#define GC_EXPORT __declspec(dllexport)
#else
#define GC_EXPORT __declspec(dllimport)
#endif  // defined(GC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GC_IMPLEMENTATION)
#define GC_EXPORT __attribute__((visibility("default")))
#else
#define GC_EXPORT
#endif  // defined(GC_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define GC_EXPORT
#endif

#endif  // COMPONENTS_GC_CORE_GC_EXPORT_H_

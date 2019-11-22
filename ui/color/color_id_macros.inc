// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following macros are used to declare both the color id enumerations and
// the stringized names of the enumeration elements for use in dump_colors. To
// stringize the element names, define STRINGIZE_COLOR_IDS prior to including
// this file. This file is intended to be included just before and just after
// the enumeration or string array declarations.

#if !defined(COLOR_ID_MACROS_DEFINED)
#define COLOR_ID_MACROS_DEFINED
#if defined(STRINGIZE_COLOR_IDS)
#define E1(enum_name) #enum_name
#define E2(enum_name, enum_value) #enum_name
#else
#define E1(enum_name) enum_name
#define E2(enum_name, enum_value) enum_name = enum_value
#endif
#define GET_E(_1, _2, macro_name, ...) macro_name
#define E(...) GET_E(__VA_ARGS__, E2, E1)(__VA_ARGS__)
#else
#undef E1
#undef E2
#undef GET_E
#undef E
#undef COLOR_ID_MACROS_DEFINED
#endif

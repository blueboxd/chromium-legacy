// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//////////////////////////////////////////////////////////////////////////////
// Based on build/linux/debian_sid_amd64-sysroot/usr/include/link.h

struct Elf64_Dyn;

#define __ELF_NATIVE_CLASS 64
#define ElfW(type) _ElfW(Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e, w, t) _ElfW_1(e, w, _##t)
#define _ElfW_1(e, w, t) e##w##t

struct MacroTest1 {
  ElfW(Dyn) * ptr_field;
};

//////////////////////////////////////////////////////////////////////////////
// Based on base/third_party/libevent/event.h

struct event;
struct event_base;

#define TAILQ_ENTRY(type)                                          \
  struct {                                                         \
    struct type* tqe_next;  /* next element */                     \
    struct type** tqe_prev; /* address of previous next element */ \
  }

struct MacroTest2 {
  TAILQ_ENTRY(event) ev_next;
  TAILQ_ENTRY(event) ev_active_next;
  TAILQ_ENTRY(event) ev_signal_next;
};

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/scoped_mach_port.h"

#include "base/apple/mach_logging.h"

namespace base::apple {
namespace internal {

// static
void SendRightTraits::Free(mach_port_t port) {
  kern_return_t kr = mach_port_deallocate(mach_task_self(), port);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "ScopedMachSendRight mach_port_deallocate";
}

// static
void ReceiveRightTraits::Free(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "ScopedMachReceiveRight mach_port_mod_refs";
}

// static
void PortSetTraits::Free(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_PORT_SET, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "ScopedMachPortSet mach_port_mod_refs";
}

}  // namespace internal

bool CreateMachPort(ScopedMachReceiveRight* receive,
                    ScopedMachSendRight* send,
                    absl::optional<mach_port_msgcount_t> queue_limit) {
  mach_port_options_t options{};
  options.flags = (send != nullptr ? MPO_INSERT_SEND_RIGHT : 0);

  if (queue_limit.has_value()) {
    options.flags |= MPO_QLIMIT;
    options.mpl.mpl_qlimit = *queue_limit;
  }

  mach_port_t name;
  kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &name);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_allocate";
  }

  kr = mach_port_insert_right(mach_task_self(), name, name, MACH_MSG_TYPE_MAKE_SEND);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_insert_right:MACH_MSG_TYPE_MAKE_SEND";
  }
  kr = mach_port_insert_right(mach_task_self(), name, name, MACH_MSG_TYPE_PORT_SEND);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_construct:MACH_MSG_TYPE_PORT_SEND";
  }

  kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, ScopedMachReceiveRight::Receiver(*receive).get());
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_port_construct";
    return false;
  }

  *ScopedMachReceiveRight::Receiver(*receive).get() = name;
  // Multiple rights are coalesced to the same name in a task, so assign the
  // send rights to the same name.
  if (send) {
    send->reset(receive->get());
  }

  return true;
}

ScopedMachSendRight RetainMachSendRight(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
  if (kr == KERN_SUCCESS) {
    return ScopedMachSendRight(port);
  }
  MACH_DLOG(ERROR, kr) << "mach_port_mod_refs +1";
  return {};
}

}  // namespace base::apple

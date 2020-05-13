// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#define CC_IPC_CC_PARAM_TRAITS_MACROS_H_

#include "base/component_export.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/touch_action.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(CC_IPC)

IPC_ENUM_TRAITS_MAX_VALUE(
    cc::OverscrollBehavior::OverscrollBehaviorType,
    cc::OverscrollBehavior::OverscrollBehaviorType::kOverscrollBehaviorTypeMax)

IPC_STRUCT_TRAITS_BEGIN(cc::OverscrollBehavior)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(cc::TouchAction, cc::TouchAction::kMax)

#endif  // CC_IPC_CC_PARAM_TRAITS_MACROS_H_

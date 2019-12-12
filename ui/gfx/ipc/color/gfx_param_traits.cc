// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/color/gfx_param_traits.h"

#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

namespace IPC {

void ParamTraits<gfx::ColorSpace>::Write(base::Pickle* m,
                                         const gfx::ColorSpace& p) {
  WriteParam(m, p.GetPrimaryID());
  WriteParam(m, p.GetTransferID());
  WriteParam(m, p.GetMatrixID());
  WriteParam(m, p.GetRangeID());
  if (p.GetPrimaryID() == gfx::ColorSpace::PrimaryID::CUSTOM) {
    skcms_Matrix3x3 custom_primary_matrix;
    p.GetPrimaryMatrix(&custom_primary_matrix);
    ParamTraits<skcms_Matrix3x3>::Write(m, custom_primary_matrix);
  }
  if (p.GetTransferID() == gfx::ColorSpace::TransferID::CUSTOM) {
    skcms_TransferFunction custom_transfer_params;
    p.GetTransferFunction(&custom_transfer_params);
    ParamTraits<skcms_TransferFunction>::Write(m, custom_transfer_params);
  }
}

bool ParamTraits<gfx::ColorSpace>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        gfx::ColorSpace* r) {
  gfx::ColorSpace::PrimaryID primaries = gfx::ColorSpace::PrimaryID::INVALID;
  if (!ReadParam(m, iter, &primaries))
    return false;
  gfx::ColorSpace::TransferID transfer = gfx::ColorSpace::TransferID::INVALID;
  if (!ReadParam(m, iter, &transfer))
    return false;
  gfx::ColorSpace::MatrixID matrix = gfx::ColorSpace::MatrixID::INVALID;
  if (!ReadParam(m, iter, &matrix))
    return false;
  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;
  if (!ReadParam(m, iter, &range))
    return false;
  skcms_Matrix3x3 custom_primary_matrix;
  if (primaries == gfx::ColorSpace::PrimaryID::CUSTOM &&
      !ParamTraits<skcms_Matrix3x3>::Read(m, iter, &custom_primary_matrix)) {
    return false;
  }
  skcms_TransferFunction custom_transfer_params;
  if (transfer == gfx::ColorSpace::TransferID::CUSTOM &&
      !ParamTraits<skcms_TransferFunction>::Read(m, iter,
                                                 &custom_transfer_params)) {
    return false;
  }
  *r = gfx::ColorSpace(
      primaries, transfer, matrix, range,
      primaries == gfx::ColorSpace::PrimaryID::CUSTOM ? &custom_primary_matrix
                                                      : nullptr,
      transfer == gfx::ColorSpace::TransferID::CUSTOM ? &custom_transfer_params
                                                      : nullptr);
  return true;
}

void ParamTraits<gfx::ColorSpace>::Log(const gfx::ColorSpace& p,
                                       std::string* l) {
  l->append("<gfx::ColorSpace>");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

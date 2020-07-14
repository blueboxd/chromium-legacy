// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/printing_param_traits.h"

#include "base/containers/flat_map.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/pickle.h"

namespace IPC {

using printing::mojom::DidPrintContentParamsPtr;

void ParamTraits<DidPrintContentParamsPtr>::Write(base::Pickle* m,
                                                  const param_type& p) {
  WriteParam(m, p->metafile_data_region);
  WriteParam(m, p->subframe_content_info);
}

bool ParamTraits<DidPrintContentParamsPtr>::Read(const base::Pickle* m,
                                                 base::PickleIterator* iter,
                                                 param_type* p) {
  bool success = true;

  base::ReadOnlySharedMemoryRegion metafile_data_region;
  success &= ReadParam(m, iter, &metafile_data_region);

  base::flat_map<uint32_t, base::UnguessableToken> subframe_content_info;
  success &= ReadParam(m, iter, &subframe_content_info);

  if (success) {
    *p = printing::mojom::DidPrintContentParams::New(
        std::move(metafile_data_region), subframe_content_info);
  }
  return success;
}

void ParamTraits<DidPrintContentParamsPtr>::Log(const param_type& p,
                                                std::string* l) {}

}  // namespace IPC
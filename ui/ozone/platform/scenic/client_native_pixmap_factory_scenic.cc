// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/client_native_pixmap_factory_scenic.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/system/sys_info.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

class ClientNativePixmapFuchsia : public gfx::ClientNativePixmap {
 public:
  explicit ClientNativePixmapFuchsia(gfx::NativePixmapHandle handle)
      : handle_(std::move(handle)) {
    DCHECK(!handle_.planes.empty());
  }

  ~ClientNativePixmapFuchsia() override {
    if (mapping_)
      Unmap();
  }

  bool Map() override {
    if (mapping_)
      return true;

    if (!handle_.planes[0].vmo) {
      NOTREACHED();
      return false;
    }

    uintptr_t addr;

    // Assume that last plane is at the end of the VMO.
    mapping_size_ = handle_.planes.back().offset + handle_.planes.back().size;

    // Verify that all planes fall within the mapped range.
    for (auto& plane : handle_.planes) {
      DCHECK_LE(plane.offset + plane.size, mapping_size_);
    }

    // Round mapping size to align with the page size.
    size_t page_size = base::SysInfo::VMAllocationGranularity();
    mapping_size_ = (mapping_size_ + page_size - 1) & ~(page_size - 1);

    zx_status_t status =
        zx::vmar::root_self()->map(0, handle_.planes[0].vmo, 0, mapping_size_,
                                   ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, &addr);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      return false;
    }
    mapping_ = reinterpret_cast<uint8_t*>(addr);

    return true;
  }

  void Unmap() override {
    DCHECK(mapping_);
    zx_status_t status = zx::vmar::root_self()->unmap(
        reinterpret_cast<uintptr_t>(mapping_), mapping_size_);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
    mapping_ = nullptr;
  }

  void* GetMemoryAddress(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    DCHECK(mapping_);
    return mapping_ + handle_.planes[plane].offset;
  }

  int GetStride(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return handle_.planes[plane].stride;
  }

 private:
  gfx::NativePixmapHandle handle_;

  uint8_t* mapping_ = nullptr;
  size_t mapping_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFuchsia);
};

class ScenicClientNativePixmapFactory : public gfx::ClientNativePixmapFactory {
 public:
  ScenicClientNativePixmapFactory() = default;
  ~ScenicClientNativePixmapFactory() override = default;

  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    if (handle.planes.empty())
      return nullptr;
    return std::make_unique<ClientNativePixmapFuchsia>(std::move(handle));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScenicClientNativePixmapFactory);
};

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryScenic() {
  return new ScenicClientNativePixmapFactory();
}

}  // namespace ui

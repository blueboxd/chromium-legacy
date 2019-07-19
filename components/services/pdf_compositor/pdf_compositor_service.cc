// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/pdf_compositor/pdf_compositor_service.h"

#include <utility>

#include <memory>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/discardable_memory.h"
#include "build/build_config.h"
#include "components/services/pdf_compositor/pdf_compositor_impl.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#elif defined(OS_MACOSX)
#include "third_party/blink/public/platform/platform.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

namespace {

void OnPdfCompositorRequest(service_manager::ServiceKeepalive* keepalive,
                            printing::mojom::PdfCompositorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<printing::PdfCompositorImpl>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

namespace printing {

PdfCompositorService::PdfCompositorService(
    service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, base::TimeDelta{}) {}

PdfCompositorService::~PdfCompositorService() {
#if defined(OS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

// static
std::unique_ptr<service_manager::Service> PdfCompositorService::Create(
    service_manager::mojom::ServiceRequest request) {
#if defined(OS_WIN)
  // Initialize direct write font proxy so skia can use it.
  content::InitializeDWriteFontProxy();
#endif
  return std::make_unique<printing::PdfCompositorService>(std::move(request));
}

void PdfCompositorService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnPdfCompositorRequest, &keepalive_));

  if (skip_initialization_for_testing_)
    return;

  // Set up discardable memory manager.
  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr manager_ptr;
  binding_.GetConnector()->BindInterface(content::mojom::kSystemServiceName,
                                         &manager_ptr);
  discardable_shared_memory_manager_ = std::make_unique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_ptr), content::UtilityThread::Get()->GetIOTaskRunner());
  DCHECK(discardable_shared_memory_manager_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // Check that we have sandbox support on this platform.
  DCHECK(blink::Platform::Current()->GetSandboxSupport());
#endif

#if defined(OS_MACOSX)
  // Check that font access is granted.
  // This doesn't do comprehensive tests to make sure fonts can work properly.
  // It is just a quick and simple check to catch things like improper sandbox
  // policy setup.
  DCHECK(SkFontMgr::RefDefault()->countFamilies());
#endif
}

void PdfCompositorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace printing

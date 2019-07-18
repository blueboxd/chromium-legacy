// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_
#define COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "skia/ext/skia_utils_base.h"

namespace font_service {

class FontServiceApp : public service_manager::Service,
                       public mojom::FontService {
 public:
  explicit FontServiceApp(service_manager::mojom::ServiceRequest request);
  ~FontServiceApp() override;

  void CreateSelf(mojom::FontServiceRequest request);

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // FontService:
  void MatchFamilyName(const std::string& family_name,
                       mojom::TypefaceStylePtr requested_style,
                       MatchFamilyNameCallback callback) override;
  void OpenStream(uint32_t id_number, OpenStreamCallback callback) override;
  void FallbackFontForCharacter(
      uint32_t character,
      const std::string& locale,
      FallbackFontForCharacterCallback callback) override;
  void FontRenderStyleForStrike(
      const std::string& family,
      uint32_t size,
      bool italic,
      bool bold,
      float device_scale_factor,
      FontRenderStyleForStrikeCallback callback) override;
  void MatchFontByPostscriptNameOrFullFontName(
      const std::string& family,
      MatchFontByPostscriptNameOrFullFontNameCallback callback) override;
  void MatchFontWithFallback(const std::string& family,
                             bool is_bold,
                             bool is_italic,
                             uint32_t charset,
                             uint32_t fallbackFamilyType,
                             MatchFontWithFallbackCallback callback) override;
  int FindOrAddPath(const SkString& path);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::FontService> bindings_;

  // We don't want to leak paths to our callers; we thus enumerate the paths of
  // fonts.
  std::vector<SkString> paths_;

  DISALLOW_COPY_AND_ASSIGN(FontServiceApp);
};

}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_

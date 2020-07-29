// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_SOURCE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_SOURCE_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chromeos {

// A data source that helps with loading resources needed by
// chrome-untrusted://telemetry-extension/ WebUI pages.
// There are two types of resources:
// 1. GRIT resourse if the resource path exist in |path_to_idr_map_|; otherwise,
// 2. Resource from the directory specified by
// |chromeos::switches::kTelemetryExtensionDirectory| command line switch.
class TelemetryExtensionUntrustedSource : public content::URLDataSource {
 public:
  static std::unique_ptr<TelemetryExtensionUntrustedSource> Create(
      std::string source);

  TelemetryExtensionUntrustedSource(const TelemetryExtensionUntrustedSource&) =
      delete;
  TelemetryExtensionUntrustedSource& operator=(
      const TelemetryExtensionUntrustedSource&) = delete;
  ~TelemetryExtensionUntrustedSource() override;

  void AddResourcePath(base::StringPiece path, int resource_id);
  void OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName directive,
                                     const std::string& value);

  // content::URLDataSource overrides:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

 private:
  explicit TelemetryExtensionUntrustedSource(const std::string& source);

  base::Optional<int> PathToIdr(const std::string& path);

  const base::FilePath root_directory_;
  const std::string source_;
  std::map<std::string, int> path_to_idr_map_;
  base::flat_map<network::mojom::CSPDirectiveName, std::string>
      csp_overrides_map_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UNTRUSTED_SOURCE_H_

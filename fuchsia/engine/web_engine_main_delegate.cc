// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_engine_main_delegate.h"

#include <utility>

#include "base/base_paths.h"
#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/browser/web_engine_browser_main.h"
#include "fuchsia/engine/browser/web_engine_content_browser_client.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia/engine/web_engine_content_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

WebEngineMainDelegate* g_current_web_engine_main_delegate = nullptr;

void InitLoggingFromCommandLine(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr") {
    settings.logging_dest = logging::LOG_TO_STDERR;
  } else {
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  }
  if (command_line.HasSwitch(switches::kLogFile)) {
    settings.logging_dest |= logging::LOG_TO_FILE;
    settings.log_file_path =
        command_line.GetSwitchValueASCII(switches::kLogFile).c_str();
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

void InitializeResourceBundle() {
  base::FilePath pak_file;
  bool result = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(result);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_engine.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

}  // namespace

// static
WebEngineMainDelegate* WebEngineMainDelegate::GetInstanceForTest() {
  return g_current_web_engine_main_delegate;
}

WebEngineMainDelegate::WebEngineMainDelegate(
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : request_(std::move(request)) {
  g_current_web_engine_main_delegate = this;
}

WebEngineMainDelegate::~WebEngineMainDelegate() = default;

bool WebEngineMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  InitLoggingFromCommandLine(*command_line);
  content_client_ = std::make_unique<WebEngineContentClient>();
  SetContentClient(content_client_.get());
  return false;
}

void WebEngineMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

int WebEngineMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

  return WebEngineBrowserMain(main_function_params);
}

content::ContentBrowserClient*
WebEngineMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_ =
      std::make_unique<WebEngineContentBrowserClient>(std::move(request_));
  return browser_client_.get();
}

content::ContentRendererClient*
WebEngineMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WebEngineContentRendererClient>();
  return renderer_client_.get();
}

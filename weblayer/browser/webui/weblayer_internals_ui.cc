// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webui/weblayer_internals_ui.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "weblayer/grit/weblayer_resources.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif

namespace weblayer {
namespace {

#if defined(OS_ANDROID)
bool g_remote_debugging_enabled = false;

const char kSocketNameFormat[] = "weblayer_devtools_remote_%d";
const char kTetheringSocketName[] = "weblayer_devtools_tethering_%d_%d";

const int kBackLog = 10;

// Factory for UnixDomainServerSocket.
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // content::DevToolsAgentHost::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    auto socket = std::make_unique<net::UnixDomainServerSocket>(
        base::BindRepeating(&content::CanUserConnectToDevTools),
        true /* use_abstract_namespace */);
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    *name = base::StringPrintf(kTetheringSocketName, getpid(),
                               ++last_tethering_socket_);
    auto socket = std::make_unique<net::UnixDomainServerSocket>(
        base::BindRepeating(&content::CanUserConnectToDevTools),
        true /* use_abstract_namespace */);
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};
#endif

}  // namespace

const char kChromeUIWebLayerHost[] = "weblayer";

WebLayerInternalsUI::WebLayerInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIWebLayerHost);
  source->AddResourcePath("weblayer_internals.js", IDR_WEBLAYER_INTERNALS_JS);
  source->AddResourcePath("weblayer_internals.mojom-lite.js",
                          IDR_WEBLAYER_INTERNALS_MOJO_JS);
  source->SetDefaultResource(IDR_WEBLAYER_INTERNALS_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
  AddHandlerToRegistry(base::BindRepeating(
      &WebLayerInternalsUI::BindPageHandler, base::Unretained(this)));
}

WebLayerInternalsUI::~WebLayerInternalsUI() {}

#if defined(OS_ANDROID)
void WebLayerInternalsUI::GetRemoteDebuggingEnabled(
    GetRemoteDebuggingEnabledCallback callback) {
  std::move(callback).Run(g_remote_debugging_enabled);
}

void WebLayerInternalsUI::SetRemoteDebuggingEnabled(bool enabled) {
  if (g_remote_debugging_enabled == enabled)
    return;

  g_remote_debugging_enabled = enabled;
  if (enabled) {
    auto factory = std::make_unique<UnixDomainServerSocketFactory>(
        base::StringPrintf(kSocketNameFormat, getpid()));
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::move(factory), base::FilePath(), base::FilePath());
  } else {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }
}
#endif

void WebLayerInternalsUI::BindPageHandler(
    mojo::PendingReceiver<weblayer_internals::mojom::PageHandler>
        pending_receiver) {
  if (receiver_.is_bound())
    receiver_.reset();

  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace weblayer

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

#include <utility>

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

ReadLaterUI::ReadLaterUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIReadLaterHost);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

ReadLaterUI::~ReadLaterUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadLaterUI)

void ReadLaterUI::BindInterface(
    mojo::PendingReceiver<read_later::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReadLaterUI::CreatePageHandler(
    mojo::PendingRemote<read_later::mojom::Page> page,
    mojo::PendingReceiver<read_later::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ReadLaterPageHandler>(std::move(receiver),
                                                         std::move(page));
}

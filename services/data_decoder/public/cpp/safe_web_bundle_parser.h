// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/web_bundle_parser.mojom.h"

namespace data_decoder {

// A class to wrap remote mojom::WebBundleParserFactory and
// mojom::WebBundleParser service.
class SafeWebBundleParser {
 public:
  SafeWebBundleParser();
  // Remaining callbacks on flight will be dropped.
  ~SafeWebBundleParser();

  // Binds |this| instance to the given |file| for subsequent parse calls.
  base::File::Error OpenFile(base::File file);

  // Binds |this| instance to the given |data_source| for subsequent parse
  // calls.
  void OpenDataSource(mojo::PendingRemote<mojom::BundleDataSource> data_source);

  // Parses metadata. See mojom::WebBundleParser::ParseMetadata for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseMetadata(mojom::WebBundleParser::ParseMetadataCallback callback);

  // Parses response. See mojom::WebBundleParser::ParseResponse for
  // details.
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     mojom::WebBundleParser::ParseResponseCallback callback);

  // Sets a callback to be called when the data_decoder service connection is
  // terminated.
  void SetDisconnectCallback(base::OnceClosure callback);

 private:
  mojom::WebBundleParserFactory* GetFactory();
  void OnDisconnect();
  void OnMetadataParsed(mojom::BundleMetadataPtr metadata,
                        mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(size_t callback_id,
                        mojom::BundleResponsePtr response,
                        mojom::BundleResponseParseErrorPtr error);

  DataDecoder data_decoder_;
  mojo::Remote<mojom::WebBundleParserFactory> factory_;
  mojo::Remote<mojom::WebBundleParser> parser_;
  mojom::WebBundleParser::ParseMetadataCallback metadata_callback_;
  base::flat_map<size_t, mojom::WebBundleParser::ParseResponseCallback>
      response_callbacks_;
  base::OnceClosure disconnect_callback_;
  size_t response_callback_next_id_ = 0;
  bool disconnected_ = true;

  DISALLOW_COPY_AND_ASSIGN(SafeWebBundleParser);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_

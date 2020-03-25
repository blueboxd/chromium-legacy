// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_METADATA_H_
#define PDF_DOCUMENT_METADATA_H_

#include <string>

namespace chrome_pdf {

// Document properties, including those specified in the document information
// dictionary (see section 14.3.3 "Document Information Dictionary" of the ISO
// 32000-1 standard), as well as other properties about the file.
// TODO(crbug.com/93619): Finish adding information dictionary fields like
// |keywords|, |creation_date|, and |mod_date|. Also add fields like |version|,
// |size_bytes|, |is_encrypted|, and |is_linearized|.
struct DocumentMetadata {
  DocumentMetadata();
  DocumentMetadata(const DocumentMetadata&) = delete;
  DocumentMetadata& operator=(const DocumentMetadata&) = delete;
  ~DocumentMetadata();

  // The document's title.
  std::string title;

  // The name of the document's creator.
  std::string author;

  // The document's subject.
  std::string subject;

  // The name of the application that created the original document.
  std::string creator;

  // If the document's format was not originally PDF, the name of the
  // application that converted the document to PDF.
  std::string producer;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_METADATA_H_

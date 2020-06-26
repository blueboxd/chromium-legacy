// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/metafile_utils.h"

#include "base/time/time.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkTime.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

#if BUILDFLAG(ENABLE_TAGGED_PDF)
// Standard attribute owners from PDF 32000-1:2008 spec, section 14.8.5.2
// (Attribute owners are kind of like "categories" for structure node
// attributes.)
const char kPDFTableAttributeOwner[] = "Table";

// Table Attributes from PDF 32000-1:2008 spec, section 14.8.5.7
const char kPDFTableCellColSpanAttribute[] = "ColSpan";
const char kPDFTableCellHeadersAttribute[] = "Headers";
const char kPDFTableCellRowSpanAttribute[] = "RowSpan";
const char kPDFTableHeaderScopeAttribute[] = "Scope";
const char kPDFTableHeaderScopeColumn[] = "Column";
const char kPDFTableHeaderScopeRow[] = "Row";
#endif  // BUILDFLAG(ENABLE_TAGGED_PDF)

SkTime::DateTime TimeToSkTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  SkTime::DateTime skdate;
  skdate.fTimeZoneMinutes = 0;
  skdate.fYear = exploded.year;
  skdate.fMonth = exploded.month;
  skdate.fDayOfWeek = exploded.day_of_week;
  skdate.fDay = exploded.day_of_month;
  skdate.fHour = exploded.hour;
  skdate.fMinute = exploded.minute;
  skdate.fSecond = exploded.second;
  return skdate;
}

sk_sp<SkPicture> GetEmptyPicture() {
  SkPictureRecorder rec;
  SkCanvas* canvas = rec.beginRecording(100, 100);
  // Add some ops whose net effects equal to a noop.
  canvas->save();
  canvas->restore();
  return rec.finishRecordingAsPicture();
}

// Convert an AXNode into a SkPDF::StructureElementNode in order to make a
// tagged (accessible) PDF. Returns true on success and false if we don't
// have enough data to build a valid tree.
bool RecursiveBuildStructureTree(const ui::AXNode* ax_node,
                                 SkPDF::StructureElementNode* tag) {
#if BUILDFLAG(ENABLE_TAGGED_PDF)
  bool valid = false;

  tag->fNodeId = ax_node->GetIntAttribute(ax::mojom::IntAttribute::kDOMNodeId);
  switch (ax_node->data().role) {
    case ax::mojom::Role::kRootWebArea:
      tag->fType = SkPDF::DocumentStructureType::kDocument;
      break;
    case ax::mojom::Role::kParagraph:
      tag->fType = SkPDF::DocumentStructureType::kP;
      break;
    case ax::mojom::Role::kGenericContainer:
      tag->fType = SkPDF::DocumentStructureType::kDiv;
      break;
    case ax::mojom::Role::kHeading:
      // TODO(dmazzoni): heading levels. https://crbug.com/1039816
      tag->fType = SkPDF::DocumentStructureType::kH;
      break;
    case ax::mojom::Role::kList:
      tag->fType = SkPDF::DocumentStructureType::kL;
      break;
    case ax::mojom::Role::kListMarker:
      tag->fType = SkPDF::DocumentStructureType::kLbl;
      break;
    case ax::mojom::Role::kListItem:
      tag->fType = SkPDF::DocumentStructureType::kLI;
      break;
    case ax::mojom::Role::kTable:
      tag->fType = SkPDF::DocumentStructureType::kTable;
      break;
    case ax::mojom::Role::kRow:
      tag->fType = SkPDF::DocumentStructureType::kTR;
      break;
    case ax::mojom::Role::kColumnHeader:
      tag->fType = SkPDF::DocumentStructureType::kTH;
      tag->fAttributes.appendName(kPDFTableAttributeOwner,
                                  kPDFTableHeaderScopeAttribute,
                                  kPDFTableHeaderScopeColumn);
      break;
    case ax::mojom::Role::kRowHeader:
      tag->fType = SkPDF::DocumentStructureType::kTH;
      tag->fAttributes.appendName(kPDFTableAttributeOwner,
                                  kPDFTableHeaderScopeAttribute,
                                  kPDFTableHeaderScopeRow);
      break;
    case ax::mojom::Role::kCell: {
      tag->fType = SkPDF::DocumentStructureType::kTD;

      // Append an attribute consisting of the string IDs of all of the
      // header cells that correspond to this table cell.
      std::vector<ui::AXNode*> header_nodes;
      ax_node->GetTableCellColHeaders(&header_nodes);
      ax_node->GetTableCellRowHeaders(&header_nodes);
      std::vector<int> header_ids;
      header_ids.reserve(header_nodes.size());
      for (ui::AXNode* header_node : header_nodes) {
        header_ids.push_back(header_node->GetIntAttribute(
            ax::mojom::IntAttribute::kDOMNodeId));
      }
      tag->fAttributes.appendNodeIdArray(
          kPDFTableAttributeOwner, kPDFTableCellHeadersAttribute, header_ids);
      break;
    }
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kImage: {
      tag->fType = SkPDF::DocumentStructureType::kFigure;
      std::string alt =
          ax_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
      tag->fAlt = SkString(alt.c_str());
      break;
    }
    case ax::mojom::Role::kStaticText:
      // Currently we're only marking text content, so we can't generate
      // a nonempty structure tree unless we have at least one kStaticText
      // node in the tree.
      tag->fType = SkPDF::DocumentStructureType::kNonStruct;
      valid = true;
      break;
    default:
      tag->fType = SkPDF::DocumentStructureType::kNonStruct;
  }

  if (ui::IsCellOrTableHeader(ax_node->data().role)) {
    base::Optional<int> row_span = ax_node->GetTableCellRowSpan();
    if (row_span.has_value()) {
      tag->fAttributes.appendInt(kPDFTableAttributeOwner,
                                 kPDFTableCellRowSpanAttribute,
                                 row_span.value());
    }
    base::Optional<int> col_span = ax_node->GetTableCellColSpan();
    if (col_span.has_value()) {
      tag->fAttributes.appendInt(kPDFTableAttributeOwner,
                                 kPDFTableCellColSpanAttribute,
                                 col_span.value());
    }
  }

  std::string lang = ax_node->GetLanguage();
  std::string parent_lang =
      ax_node->parent() ? ax_node->parent()->GetLanguage() : "";
  if (!lang.empty() && lang != parent_lang)
    tag->fLang = lang.c_str();

  size_t children_count = ax_node->GetUnignoredChildCount();
  tag->fChildVector.resize(children_count);
  for (size_t i = 0; i < children_count; i++) {
    tag->fChildVector[i] = std::make_unique<SkPDF::StructureElementNode>();
    bool success = RecursiveBuildStructureTree(
        ax_node->GetUnignoredChildAtIndex(i), tag->fChildVector[i].get());

    if (success)
      valid = true;
  }

  return valid;
#else  // BUILDFLAG(ENABLE_TAGGED_PDF)
  return false;
#endif
}

}  // namespace

namespace printing {

sk_sp<SkDocument> MakePdfDocument(const std::string& creator,
                                  const ui::AXTreeUpdate& accessibility_tree,
                                  SkWStream* stream) {
  SkPDF::Metadata metadata;
  SkTime::DateTime now = TimeToSkTime(base::Time::Now());
  metadata.fCreation = now;
  metadata.fModified = now;
  metadata.fCreator = creator.empty()
                          ? SkString("Chromium")
                          : SkString(creator.c_str(), creator.size());
  metadata.fRasterDPI = 300.0f;

  SkPDF::StructureElementNode tag_root = {};
  if (!accessibility_tree.nodes.empty()) {
    ui::AXTree tree(accessibility_tree);
    if (RecursiveBuildStructureTree(tree.root(), &tag_root))
      metadata.fStructureElementTreeRoot = &tag_root;
  }

  return SkPDF::MakeDocument(stream, metadata);
}

sk_sp<SkData> SerializeOopPicture(SkPicture* pic, void* ctx) {
  const ContentToProxyIdMap* context =
      reinterpret_cast<const ContentToProxyIdMap*>(ctx);
  uint32_t pic_id = pic->uniqueID();
  auto iter = context->find(pic_id);
  if (iter == context->end())
    return nullptr;

  return SkData::MakeWithCopy(&pic_id, sizeof(pic_id));
}

sk_sp<SkPicture> DeserializeOopPicture(const void* data,
                                       size_t length,
                                       void* ctx) {
  uint32_t pic_id;
  if (length < sizeof(pic_id)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return GetEmptyPicture();
  }
  memcpy(&pic_id, data, sizeof(pic_id));

  auto* context = reinterpret_cast<PictureDeserializationContext*>(ctx);
  auto iter = context->find(pic_id);
  if (iter == context->end() || !iter->second) {
    // When we don't have the out-of-process picture available, we return
    // an empty picture. Returning a nullptr will cause the deserialization
    // crash.
    return GetEmptyPicture();
  }
  return iter->second;
}

sk_sp<SkData> SerializeOopTypeface(SkTypeface* typeface, void* ctx) {
  auto* context = reinterpret_cast<TypefaceSerializationContext*>(ctx);
  SkFontID typeface_id = typeface->uniqueID();
  bool data_included = context->insert(typeface_id).second;

  // Need the typeface ID to identify the desired typeface.  Include an
  // indicator for when typeface data actually follows vs. when the typeface
  // should already exist in a cache when deserializing.
  SkDynamicMemoryWStream stream;
  stream.write32(typeface_id);
  stream.writeBool(data_included);
  if (data_included) {
    typeface->serialize(&stream, SkTypeface::SerializeBehavior::kDoIncludeData);
  }
  return stream.detachAsData();
}

sk_sp<SkTypeface> DeserializeOopTypeface(const void* data,
                                         size_t length,
                                         void* ctx) {
  SkStream* stream = *(reinterpret_cast<SkStream**>(const_cast<void*>(data)));
  if (length < sizeof(stream)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return nullptr;
  }

  SkFontID id;
  if (!stream->readU32(&id)) {
    return nullptr;
  }
  bool data_included;
  if (!stream->readBool(&data_included)) {
    return nullptr;
  }

  auto* context = reinterpret_cast<TypefaceDeserializationContext*>(ctx);
  auto iter = context->find(id);
  if (iter != context->end()) {
    DCHECK(!data_included);
    return iter->second;
  }

  // Typeface not encountered before, expect it to be present in the stream.
  DCHECK(data_included);
  sk_sp<SkTypeface> typeface = SkTypeface::MakeDeserialize(stream);
  context->emplace(id, typeface);
  return typeface;
}

SkSerialProcs SerializationProcs(PictureSerializationContext* picture_ctx,
                                 TypefaceSerializationContext* typeface_ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializeOopPicture;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = SerializeOopTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  return procs;
}

SkDeserialProcs DeserializationProcs(
    PictureDeserializationContext* picture_ctx,
    TypefaceDeserializationContext* typeface_ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializeOopPicture;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = DeserializeOopTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  return procs;
}

}  // namespace printing

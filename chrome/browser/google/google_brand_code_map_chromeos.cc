// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_brand_code_map_chromeos.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"

namespace google_brand {
namespace chromeos {

std::string GetRlzBrandCode(
    const std::string& static_brand_code,
    base::Optional<policy::MarketSegment> market_segment) {
  struct BrandCodeValueEntry {
    const char* unenrolled_brand_code;
    const char* education_enrolled_brand_code;
    const char* enterprise_enrolled_brand_code;
  };
  static const base::NoDestructor<
      base::flat_map<std::string, BrandCodeValueEntry>>
      kBrandCodeMap({{"ACAC", {"CFZM", "BEUH", "GUTN"}},
                     {"ACAG", {"KSOU", "MUHR", "YYJR"}},
                     {"ACAH", {"KEFG", "RYNH", "HHAZ"}},
                     {"ACAJ", {"KVPC", "UHAI", "CPNG"}},
                     {"ACAM", {"HBCZ", "ZGSZ", "MFUO"}},
                     {"ACAO", {"MWDF", "BNNY", "SYIY"}},
                     {"ACAP", {"LKNW", "SVFL", "FGKR"}},
                     {"ACAR", {"EAQE", "UHHJ", "ZYFW"}},
                     {"ACAT", {"RJNJ", "CKCB", "VHGI"}},
                     {"ACAV", {"TTSD", "XTQQ", "TIQC"}},
                     {"ACAY", {"HKDC", "RYKK", "KSIY"}},
                     {"ACBA", {"TVZD", "HLQR", "DOWV"}},
                     {"ADGK", {"PKUQ", "AEMI", "CUUL"}},
                     {"ASCT", {"CTRF", "LBBD", "YBND"}},
                     {"ASUA", {"IEIT", "JAIV", "MURN"}},
                     {"ASUD", {"QLMM", "CRUA", "JSID"}},
                     {"ASUE", {"XLEN", "KECH", "HBGX"}},
                     {"ASUF", {"IVGE", "VNTM", "XELD"}},
                     {"ASUJ", {"HJUL", "XWWL", "WSCY"}},
                     {"ASUK", {"RGUX", "OXBQ", "LDTL"}},
                     {"AYMH", {"BBMB", "VBWP", "BVTP"}},
                     {"BAQN", {"YJJJ", "LDCA", "QSJF"}},
                     {"BCOL", {"YJDV", "GSIC", "BAUL"}},
                     {"BDIW", {"UDUG", "TRYQ", "PWFV"}},
                     {"CQPQ", {"GATZ", "QAVU", "WRXC"}},
                     {"CYQR", {"XGJJ", "DRMC", "RUQD"}},
                     {"CYSQ", {"NHHD", "TAVM", "FHSA"}},
                     {"DEAA", {"HXUG", "BJUN", "IYTV"}},
                     {"DEAC", {"DSMM", "IXET", "KQDV"}},
                     {"DEAF", {"TATK", "RWXF", "DQDT"}},
                     {"DEAG", {"JFEX", "CVLN", "UFWN"}},
                     {"DRYI", {"LWTQ", "OLEY", "NWUA"}},
                     {"DVUG", {"HJHV", "KPAH", "DCQS"}},
                     {"FQPJ", {"ZTQG", "ZNEO", "LYMZ"}},
                     {"FSFR", {"ZDAR", "BERM", "COKX"}},
                     {"FSGY", {"PJQC", "RHZW", "POVI"}},
                     {"FWVK", {"MUTD", "GWKK", "SQSC"}},
                     {"GJZV", {"BUSA", "GIOS", "UYOM"}},
                     {"HOMH", {"BXHI", "WXYD", "VRZY"}},
                     {"HOWM", {"MJNG", "XPYN", "IRWY"}},
                     {"HPZP", {"NQDY", "QIMT", "QKAK"}},
                     {"HPZQ", {"XGER", "OLTF", "DVQA"}},
                     {"HPZS", {"QRFK", "SQGI", "VESI"}},
                     {"HPZT", {"IUCU", "WDAV", "LOLH"}},
                     {"HPZV", {"WAFN", "PQVW", "MJVM"}},
                     {"HPZY", {"RAWP", "CNRC", "TPIA"}},
                     {"IHZG", {"MLLN", "EZTK", "GJEJ"}},
                     {"JBPA", {"VUZL", "XYPI", "XOWE"}},
                     {"JLRH", {"SAMJ", "GLJZ", "SKTN"}},
                     {"JYXK", {"USZT", "XXPU", "LJHH"}},
                     {"LASN", {"ILWC", "BQYG", "RROZ"}},
                     {"LEAC", {"DMEA", "EXWD", "PBTU"}},
                     {"LEAE", {"QFVM", "GACH", "BMXB"}},
                     {"LEAG", {"XTLW", "WLQO", "QVKP"}},
                     {"LEAH", {"QIDR", "XBTQ", "QYUO"}},
                     {"LEAI", {"KCSV", "PRBF", "FVDO"}},
                     {"LEAK", {"CGWM", "ZLOS", "JGTD"}},
                     {"LEAL", {"EYPX", "SOCH", "PFPW"}},
                     {"LEAO", {"MKOE", "YJSI", "QQMN"}},
                     {"LEAP", {"AEZG", "JOYE", "JHWK"}},
                     {"MAII", {"EOHR", "XZOT", "VJJS"}},
                     {"MCDN", {"BAOV", "GLVV", "XHGO"}},
                     {"MCOO", {"IPNW", "CRSK", "QTAX"}},
                     {"MNZG", {"PPTP", "OFXE", "ROJJ"}},
                     {"NBQS", {"KMJF", "MFWA", "UWRX"}},
                     {"NOMD", {"GZLV", "UNZR", "FVOP"}},
                     {"NPEC", {"BMGD", "YETH", "XAWJ"}},
                     {"OFPE", {"YFOO", "UIGY", "PFGZ"}},
                     {"OKWC", {"RGFB", "UPFP", "HUVK"}},
                     {"PAZD", {"VARX", "KZSU", "WPLH"}},
                     {"PGQF", {"USPJ", "SFKO", "KNBH"}},
                     {"PXDO", {"ZXCF", "TQWC", "HOAL"}},
                     {"RVRM", {"MZJU", "IGXP", "DSJP"}},
                     {"SMAE", {"SUUV", "QXWL", "LYKX"}},
                     {"SMAF", {"HKPA", "NFCE", "UBOP"}},
                     {"SMAH", {"EXLB", "YYYY", "LLLA"}},
                     {"SMAI", {"PPDO", "ISMM", "BKNT"}},
                     {"SMAJ", {"PVCB", "UCIK", "XVBK"}},
                     {"SMAK", {"WOMZ", "OHAX", "JSTF"}},
                     {"SMAL", {"OWLX", "YXSA", "TXJR"}},
                     {"TAAB", {"ZBMY", "NYDT", "CXYZ"}},
                     {"TKER", {"KOSM", "IUCL", "LIIM"}},
                     {"UGAY", {"YDHM", "HVCY", "ILHO"}},
                     {"UMAU", {"FKAK", "JCTZ", "GDUU"}},
                     {"VEUT", {"JDFA", "ALIR", "DDJM"}},
                     {"VHUH", {"JYDF", "SFJY", "JMBU"}},
                     {"WBZQ", {"LAYK", "LQDM", "QBFV"}},
                     {"WJOZ", {"BASQ", "BRTL", "CQAV"}},
                     {"XVTK", {"TMUU", "BTWW", "THQH"}},
                     {"XVYQ", {"UAVB", "OEMI", "VQVK"}},
                     {"XWJE", {"KDZI", "IYPJ", "ERIM"}},
                     {"YHYU", {"CDLM", "QDXQ", "HPTE"}},
                     {"YMMU", {"ZVIA", "CFKN", "ERLO"}},
                     {"ZDKS", {"UBRP", "AWQF", "GOVG"}},
                     {"ZFCZ", {"JQUA", "SEEH", "RJVV"}},
                     {"ZSKM", {"JPEZ", "FTUS", "ZFUF"}},
                     {"ZZAB", {"WVIK", "IUXK", "ZCIK"}},
                     {"ZZAD", {"KSTH", "CBJY", "TSID"}},
                     {"ZZAF", {"OTWH", "RRNB", "VNXA"}}});

  const auto it = kBrandCodeMap->find(static_brand_code);
  if (it == kBrandCodeMap->end())
    return static_brand_code;
  const auto& entry = it->second;
  // An empty value indicates the device is not enrolled.
  if (!market_segment.has_value())
    return entry.unenrolled_brand_code;

  switch (market_segment.value()) {
    case policy::MarketSegment::EDUCATION:
      return entry.education_enrolled_brand_code;
    case policy::MarketSegment::ENTERPRISE:
    case policy::MarketSegment::UNKNOWN:
      // If the device is enrolled but market segment is unknown, it's fine to
      // treat it as enterprise enrolled.
      return entry.enterprise_enrolled_brand_code;
  }
  NOTREACHED();
  return static_brand_code;
}

}  // namespace chromeos
}  // namespace google_brand

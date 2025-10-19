#include "PointerTracker.h"

#include <Utils/ExtractionContext.h>

using namespace DyldExtractor;
using namespace Provider;

template <class P>
bool PointerTracker<P>::MappingSlideInfo::containsAddr(
    const uint64_t addr) const {
  return addr >= address && addr < address + size;
}

template <class P>
const uint8_t *
PointerTracker<P>::MappingSlideInfo::convertAddr(const uint64_t addr) const {
  return (addr - address) + data;
}

template <class P>
PointerTracker<P>::PointerTracker(
    const Dyld::Context &dCtx,
    std::optional<std::shared_ptr<spdlog::logger>> logger)
    : dCtx(&dCtx), logger(logger) {
  fillMappings();
}

template <class P>
PointerTracker<P>::PtrT PointerTracker<P>::slideP(const PtrT addr) const {
  for (auto &map : mappings) {
    if (!map.containsAddr(addr)) {
      continue;
    }
    auto ptr = map.convertAddr(addr);

    switch (map.slideInfoVersion) {
    case 1: {
      return *(PtrT *)ptr;
    }
    case 2: {
      auto slideInfo = (dyld_cache_slide_info2 *)map.slideInfo;
      auto val = *(PtrT *)ptr & ~slideInfo->delta_mask;
      if (val != 0) {
        val += slideInfo->value_add;
      }
      return (PtrT)val;
    }
    case 3: {
      auto ptrInfo = (dyld_cache_slide_pointer3 *)ptr;
      if (ptrInfo->auth.authenticated) {
        auto slideInfo = (dyld_cache_slide_info3 *)map.slideInfo;
        return (PtrT)ptrInfo->auth.offsetFromSharedCacheBase +
               (PtrT)slideInfo->auth_value_add;
      } else {
        uint64_t value51 = ptrInfo->plain.pointerValue;
        uint64_t top8Bits = value51 & 0x0007F80000000000ULL;
        uint64_t bottom43Bits = value51 & 0x000007FFFFFFFFFFULL;
        return (PtrT)(top8Bits << 13) | (PtrT)bottom43Bits;
      }
    }
    case 4: {
      auto slideInfo = (dyld_cache_slide_info4 *)map.slideInfo;
      auto newValue = *(uint32_t *)ptr & ~(slideInfo->delta_mask);
      return (PtrT)newValue + (PtrT)slideInfo->value_add;
    }
    case 5: {
        auto slideInfo = (dyld_cache_slide_info5*)map.slideInfo;
        return (PtrT)addr + (PtrT)slideInfo->value_add;
    }
    default: {
      if (logger) {
        SPDLOG_LOGGER_ERROR(*logger, "Unknown slide info version {}.",
                            map.slideInfoVersion);
      }
      return 0;
    }
    }
  }

  return 0;
}

template <class P>
void PointerTracker<P>::add(const PtrT addr, const PtrT target) {
  pointers[addr] = target;
}

template <class P>
void PointerTracker<P>::addAuth(const PtrT addr, AuthData data) {
  authData[addr] = data;
}

template <class P>
void PointerTracker<P>::copyAuth(const PtrT addr, const PtrT sAddr) {
  for (const auto mapI : authMappings) {
    const auto &map = mappings.at(mapI);
    if (map.containsAddr(sAddr)) {

      auto p = (dyld_cache_slide_pointer3 *)map.convertAddr(sAddr);
      if (p->auth.authenticated) {
        addAuth(addr,
                {(uint16_t)p->auth.diversityData,
                 (bool)p->auth.hasAddressDiversity, (uint8_t)p->auth.key});
      }
      break;
    }
  }
}

template <class P>
void PointerTracker<P>::removePointers(const PtrT start, const PtrT end) {
  // Remove from pointers, auth, and bind
  pointers.erase(pointers.lower_bound(start), pointers.upper_bound(end));
  authData.erase(authData.lower_bound(start), authData.upper_bound(end));
  bindData.erase(bindData.lower_bound(start), bindData.upper_bound(end));
}

template <class P>
void PointerTracker<P>::addBind(const PtrT addr,
                                std::shared_ptr<SymbolicInfo> data) {
  bindData[addr] = data;
}

template <class P>
const std::vector<typename PointerTracker<P>::MappingSlideInfo> &
PointerTracker<P>::getMappings() const {
  return mappings;
}

template <class P>
std::vector<const typename PointerTracker<P>::MappingSlideInfo *>
PointerTracker<P>::getSlideMappings() const {
  std::vector<const MappingSlideInfo *> results;
  results.reserve(mappings.size());
  for (const auto &mapI : slideMappings) {
    results.emplace_back(&mappings[mapI]);
  }
  return results;
}

template <class P>
const std::map<typename PointerTracker<P>::PtrT,
               typename PointerTracker<P>::PtrT> &
PointerTracker<P>::getPointers() const {
  return pointers;
}

template <class P>
const std::map<typename PointerTracker<P>::PtrT,
               typename PointerTracker<P>::AuthData> &
PointerTracker<P>::getAuths() const {
  return authData;
}

template <class P>
const std::map<typename PointerTracker<P>::PtrT,
               std::shared_ptr<SymbolicInfo>> &
PointerTracker<P>::getBinds() const {
  return bindData;
}

template <class P> uint32_t PointerTracker<P>::getPageSize() const {
  const auto slideMaps = getSlideMappings();
  if (!slideMaps.size()) {
    if (logger) {
      SPDLOG_LOGGER_ERROR(*logger, "No slide info to infer pagesize.");
    }
    return 0x1000;
  }

  const auto map = *slideMaps.begin();
  switch (map->slideInfoVersion) {
  case 1:
    // Assume 0x1000
    return 0x1000;
  case 2:
  case 3:
  case 4:
  case 5:{
    // page size in second uint32_t field
    auto pageSize = reinterpret_cast<const uint32_t *>(map->slideInfo)[1];
    return pageSize;
  }
  default: {
    if (logger) {
      SPDLOG_LOGGER_WARN(*logger, "Unknown slide info version {}.",
                         map->slideInfoVersion);
    }
    return 0x1000;
  }
  }
}

template <class P> void PointerTracker<P>::fillMappings() {
  if (dCtx->header->slideInfoOffsetUnused) {
    // Assume legacy case with no sub caches, and only one slide info
    auto maps =
        (dyld_cache_mapping_info *)(dCtx->file + dCtx->header->mappingOffset);

    // First mapping doesn't have slide info
    mappings.emplace_back(dCtx->file + maps->fileOffset, maps->address,
                          maps->size, 0, nullptr);

    // slide info corresponds to the second mapping
    auto map2 = maps + 1;
    auto slideInfo = dCtx->file + dCtx->header->slideInfoOffsetUnused;
    uint32_t slideVer = *(uint32_t *)slideInfo;
    mappings.emplace_back(dCtx->file + map2->fileOffset, map2->address,
                          map2->size, slideVer, slideInfo);
    slideMappings.push_back(1);
    if (slideVer == 3) {
      authMappings.push_back(1);
    }

    // Add other mappings
    for (uint32_t i = 2; i < dCtx->header->mappingCount; i++) {
      auto map = maps + i;
      mappings.emplace_back(dCtx->file + map->address, map->address, map->size,
                            0, nullptr);
    }
    return;
  }

  if (!dCtx->headerContainsMember(
          offsetof(dyld_cache_header, mappingWithSlideOffset))) {
    if (logger) {
      SPDLOG_LOGGER_ERROR(*logger, "Unable to get mapping and slide info.");
    }
    return;
  }

  // Get all mappings from all caches
  auto extendInfo = [this](const Dyld::Context &ctx) {
    if (!ctx.header->mappingWithSlideCount) {
      return;
    }
    auto start = (dyld_cache_mapping_and_slide_info
                      *)(ctx.file + ctx.header->mappingWithSlideOffset);
    auto end = start + ctx.header->mappingWithSlideCount;
    for (auto i = start; i < end; i++) {
      if (i->slideInfoFileOffset) {
        auto slideInfo = ctx.file + i->slideInfoFileOffset;
        auto slideVer = *(uint32_t *)slideInfo;
        mappings.emplace_back(ctx.file + i->fileOffset, i->address, i->size,
                              slideVer, slideInfo);
      } else {
        mappings.emplace_back(ctx.file + i->fileOffset, i->address, i->size, 0,
                              nullptr);
      }
    }
  };

  extendInfo(*dCtx);
  for (auto &ctx : dCtx->subcaches) {
    extendInfo(ctx);
  }

  // fill other mappings as mappings should be constant now
  for (int i = 0; i < mappings.size(); i++) {
    const auto &map = mappings.at(i);
    if (map.slideInfo != nullptr) {
      slideMappings.push_back(i);
    }
    if (map.slideInfoVersion == 3) {
      authMappings.push_back(i);
    }
  }
}

template class Provider::PointerTracker<Utils::Arch::Pointer32>;
template class Provider::PointerTracker<Utils::Arch::Pointer64>;

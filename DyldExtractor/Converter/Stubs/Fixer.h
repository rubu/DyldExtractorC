#ifndef __CONVERTER_STUBS_FIXER__
#define __CONVERTER_STUBS_FIXER__

#include "Arm64Fixer.h"
#include "ArmFixer.h"
#include "SymbolPointerCache.h"
#include <Utils/ExtractionContext.h>

namespace DyldExtractor::Converter::Stubs {

template <class A> class Fixer {
  friend class SymbolPointerCache<A>;
  friend class Arm64Fixer<A>;
  friend class ArmFixer;
  using P = A::P;
  using PtrT = P::PtrT;
  using LETrackerTag = Provider::LinkeditTracker<P>::Tag;
  using STSymbolType = Provider::SymbolTableTracker<P>::SymbolType;

public:
  Fixer(Utils::ExtractionContext<A> &eCtx);
  void fix();

private:
  void checkIndirectEntries();
  void fixIndirectEntries();
  void bindPointers();

  bool isInCodeRegions(PtrT addr);

  const Dyld::Context &dCtx;
  Macho::Context<false, P> &mCtx;
  Provider::Accelerator<P> &accelerator;
  Provider::LinkeditTracker<P> &leTracker;
  Provider::SymbolTableTracker<P> &stTracker;

  std::optional<Arm64Utils<A>> arm64Utils;
  std::optional<Arm64Fixer<A>> arm64Fixer;

  std::optional<ArmFixer> armFixer;

public:
    Provider::ActivityLogger &activity;
    std::shared_ptr<spdlog::logger> logger;
    Provider::BindInfo<P> &bindInfo;
    Provider::Disassembler<A> &disasm;
    Provider::PointerTracker<P> &ptrTracker;
    Provider::Symbolizer<A> &symbolizer;
    Utils::ExtractionContext<A> &eCtx;
    SymbolPointerCache<A> ptrCache;
    std::optional<ArmUtils> armUtils;
};

} // namespace DyldExtractor::Converter::Stubs

#endif // __CONVERTER_STUBS_FIXER__

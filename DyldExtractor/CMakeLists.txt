cmake_minimum_required(VERSION 3.12)

set(CONVERTER_SOURCES Converter/Linkedit/Encoder/BindingV1.cpp
	Converter/Linkedit/Encoder/Chained.cpp
	Converter/Linkedit/Encoder/LegacyGenerator.cpp
	Converter/Linkedit/Encoder/RebaseV1.cpp
	Converter/Linkedit/MetadataGenerator.cpp
	Converter/Linkedit/Optimizer.cpp
	Converter/Objc/Objc.cpp
	Converter/Objc/Placer.cpp
	Converter/Objc/Walker.cpp
	Converter/Stubs/Arm64Fixer.cpp
	Converter/Stubs/Arm64Utils.cpp
	Converter/Stubs/ArmFixer.cpp
	Converter/Stubs/ArmUtils.cpp
	Converter/Stubs/Fixer.cpp
	Converter/Stubs/SymbolPointerCache.cpp
	Converter/OffsetOptimizer.cpp
	Converter/Slide.cpp
)

set(CONVERTER_HEADERS Converter/Linkedit/Encoder/BindingV1.h
	Converter/Linkedit/Encoder/Chained.h
	Converter/Linkedit/Encoder/LegacyGenerator.h
	Converter/Linkedit/Encoder/RebaseV1.h
	Converter/Linkedit/MetadataGenerator.h
	Converter/Linkedit/Optimizer.h
	Converter/Objc/Objc.h
	Converter/Objc/Placer.h
	Converter/Objc/Walker.h
	Converter/Stubs/Arm64Fixer.h
	Converter/Stubs/Arm64Utils.h
	Converter/Stubs/ArmFixer.h
	Converter/Stubs/ArmUtils.h
	Converter/Stubs/Fixer.h
	Converter/Stubs/SymbolPointerCache.h
	Converter/OffsetOptimizer.h
	Converter/Slide.h
)

set(PROVIDER_HEADERS Provider/Accelerator.h
	Provider/ActivityLogger.h
	Provider/BindInfo.h
	Provider/Disassembler.h
	Provider/ExtraData.h
	Provider/FunctionTracker.h
	Provider/LinkeditTracker.h
	Provider/PointerTracker.h
	Provider/Symbolizer.h
	Provider/SymbolTableTracker.h
	Provider/Validator.ch
)

set(PROVIDER_SOURCES Provider/ActivityLogger.cpp
	Provider/BindInfo.cpp
	Provider/Disassembler.cpp
	Provider/ExtraData.cpp
	Provider/FunctionTracker.cpp
	Provider/LinkeditTracker.cpp
	Provider/PointerTracker.cpp
	Provider/Symbolizer.cpp
	Provider/SymbolTableTracker.cpp
	Provider/Validator.cpp
)

source_group("Header Files\\Converter" FILES ${CONVERTER_HEADERS})
source_group("Source Files\\Converter" FILES ${CONVERTER_SOURCES})
source_group("Header Files\\Provider" FILES ${PROVIDER_HEADERS})
source_group("Source Files\\Provider" FILES ${PROVIDER_SOURCES})

add_library(DyldExtractor
	${CONVERTER_SOURCES}
	${CONVERTER_HEADERS}
	Dyld/Context.cpp
	Macho/Context.cpp
	${PROVIDER_SOURCES}
	Utils/ExtractionContext.cpp
	Utils/Leb128.cpp
	../External/headers/dyld/dyld_cache_format.h
	../External/headers/mach-o/fixup-chains.h
)

target_link_libraries(DyldExtractor PUBLIC ${Boost_LIBRARIES})
target_include_directories(DyldExtractor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(DyldExtractor PUBLIC .)

target_link_libraries(DyldExtractor PRIVATE spdlog::spdlog)
target_link_libraries(DyldExtractor PRIVATE fmt::fmt)
target_link_libraries(DyldExtractor PRIVATE capstone::capstone)

include_directories(
        "${CMAKE_SOURCE_DIR}"
        ".."
        ${GLMINC_PREFIX}
        ${SHADERC_SRC}/third_party/spirv-tools/include
        ${SHADERC_SRC}/third_party/spirv-tools/include/spirv-tools
        "${CMAKE_CURRENT_BINARY_DIR}" # for generated spvnum files
)

add_library(androidndkgif STATIC
        BaseGifEncoder.cpp
        BaseGifEncoder.h
        BitWritingBlock.cpp
        BitWritingBlock.h
        GCTGifEncoder.cpp
        GCTGifEncoder.h
        FastGifEncoder.cpp
        FastGifEncoder.h
        )

target_link_libraries(androidndkgif
        android
        log
        )
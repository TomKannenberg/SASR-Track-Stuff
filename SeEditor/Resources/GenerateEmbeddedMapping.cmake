string(REPLACE "\"" "" MAPPING_GC_PATH "${MAPPING_GC_PATH}")
string(REPLACE "\"" "" MAPPING_GS3_PATH "${MAPPING_GS3_PATH}")
string(REPLACE "\"" "" EMBEDDED_MAPPING_HEADER "${EMBEDDED_MAPPING_HEADER}")

file(READ "${MAPPING_GC_PATH}" MAPPING_GC_TEXT)
set(MAPPING_TEXT "${MAPPING_GC_TEXT}")
if (MAPPING_GS3_PATH AND EXISTS "${MAPPING_GS3_PATH}")
    file(READ "${MAPPING_GS3_PATH}" MAPPING_GS3_TEXT)
    string(APPEND MAPPING_TEXT "\n")
    string(APPEND MAPPING_TEXT "${MAPPING_GS3_TEXT}")
endif()

# MSVC limits single string literal size; split into small raw-string chunks.
set(CHUNK_SIZE 8000)
string(LENGTH "${MAPPING_TEXT}" TOTAL_LEN)
set(POS 0)
set(INDEX 0)

file(WRITE "${EMBEDDED_MAPPING_HEADER}"
"#pragma once\n"
"#include <cstddef>\n"
"inline constexpr char kEmbeddedMapping[] =\n"
)

while(POS LESS TOTAL_LEN)
    string(SUBSTRING "${MAPPING_TEXT}" ${POS} ${CHUNK_SIZE} CHUNK_TEXT)
    file(APPEND "${EMBEDDED_MAPPING_HEADER}" "R\"MAP${INDEX}(\n")
    file(APPEND "${EMBEDDED_MAPPING_HEADER}" "${CHUNK_TEXT}")
    file(APPEND "${EMBEDDED_MAPPING_HEADER}" "\n)MAP${INDEX}\"\n")
    math(EXPR POS "${POS} + ${CHUNK_SIZE}")
    math(EXPR INDEX "${INDEX} + 1")
endwhile()

file(APPEND "${EMBEDDED_MAPPING_HEADER}"
";\ninline constexpr std::size_t kEmbeddedMappingSize = sizeof(kEmbeddedMapping) - 1;\n"
)

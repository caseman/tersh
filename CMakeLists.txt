cmake_minimum_required(VERSION 3.10)

project(tersh VERSION 0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

include_directories(
    include
    src
    lib/vtparse
)
configure_file(src/tersh.h.in include/tersh.h)
add_library(vec STATIC src/vec.h src/vec.c)
add_library(lineedit STATIC src/lineedit.h src/lineedit.c)
add_library(vtparse STATIC 
    lib/vtparse/vtparse.h 
    lib/vtparse/vtparse.c 
    lib/vtparse/vtparse_table.h
    lib/vtparse/vtparse_table.c 
)

set(RESOURCE_FILES
)

add_executable(tersh
    src/tersh.c
    src/vterm.c
    ${RESOURCE_FILES}
)

IF(APPLE)
    # Build app bundle
    set(BEARLIBTERMINAL_LIB "${PROJECT_SOURCE_DIR}/lib/libBearLibTerminal.dylib")
    # TODO icon and plist
    set_target_properties(tersh PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_FRAMEWORK_IDENTIFIER org.caseman.tersh
        RESOURCE "${RESOURCE_FILES}"
    )
    set(BUNDLE_FRAMEWORK_PATH "${CMAKE_BINARY_DIR}/tersh.app/Contents/MacOS")
    file(COPY ${BEARLIBTERMINAL_LIB} DESTINATION ${BUNDLE_FRAMEWORK_PATH}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE
    )
    # target_link_libraries(tersh "-framework Foundation" "-framework Cocoa" objc)
ENDIF(APPLE)
# TODO Linux, etc.

target_link_libraries(tersh vec lineedit vtparse
    ${BEARLIBTERMINAL_LIB}
)
target_include_directories(tersh PUBLIC
    "${PROJECT_BINARY_DIR}/include"
)

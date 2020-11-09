cmake_minimum_required(VERSION 3.10)

project(tersh VERSION 0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_command(OUTPUT tags
    COMMAND ctags -R .
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

include_directories(
    include
    src
    lib/mrsh/include
)
configure_file(src/tersh.h.in include/tersh.h)
add_library(vtparse STATIC 
    lib/vtparse/vtparse.h 
    lib/vtparse/vtparse.c 
    lib/vtparse/vtparse_table.h
    lib/vtparse/vtparse_table.c 
)
add_library(mrsh STATIC 
    lib/mrsh/arithm.c
    lib/mrsh/array.c
    lib/mrsh/ast_print.c
    lib/mrsh/ast.c
    lib/mrsh/buffer.c
    lib/mrsh/builtin/alias.c
    lib/mrsh/builtin/bg.c
    lib/mrsh/builtin/break.c
    lib/mrsh/builtin/builtin.c
    lib/mrsh/builtin/cd.c
    lib/mrsh/builtin/colon.c
    lib/mrsh/builtin/command.c
    lib/mrsh/builtin/dot.c
    lib/mrsh/builtin/eval.c
    lib/mrsh/builtin/exec.c
    lib/mrsh/builtin/exit.c
    lib/mrsh/builtin/export.c
    lib/mrsh/builtin/false.c
    lib/mrsh/builtin/fg.c
    lib/mrsh/builtin/getopts.c
    lib/mrsh/builtin/hash.c
    lib/mrsh/builtin/jobs.c
    lib/mrsh/builtin/pwd.c
    lib/mrsh/builtin/read.c
    lib/mrsh/builtin/return.c
    lib/mrsh/builtin/set.c
    lib/mrsh/builtin/shift.c
    lib/mrsh/builtin/times.c
    lib/mrsh/builtin/trap.c
    lib/mrsh/builtin/true.c
    lib/mrsh/builtin/type.c
    lib/mrsh/builtin/ulimit.c
    lib/mrsh/builtin/umask.c
    lib/mrsh/builtin/unalias.c
    lib/mrsh/builtin/unset.c
    lib/mrsh/builtin/unspecified.c
    lib/mrsh/builtin/wait.c
    lib/mrsh/getopt.c
    lib/mrsh/hashtable.c
    lib/mrsh/parser/arithm.c
    lib/mrsh/parser/parser.c
    lib/mrsh/parser/program.c
    lib/mrsh/parser/word.c
    lib/mrsh/shell/arithm.c
    lib/mrsh/shell/entry.c
    lib/mrsh/shell/job.c
    lib/mrsh/shell/path.c
    lib/mrsh/shell/process.c
    lib/mrsh/shell/redir.c
    lib/mrsh/shell/shell.c
    lib/mrsh/shell/task/pipeline.c
    lib/mrsh/shell/task/simple_command.c
    lib/mrsh/shell/task/task.c
    lib/mrsh/shell/task/word.c
    lib/mrsh/shell/trap.c
    lib/mrsh/shell/word.c
)

set(RESOURCE_FILES
    res/spinner.png
)

add_executable(tersh
    src/tersh.c
    src/vec.c
    src/smalloc.c
    src/lineedit.c
    src/poller.c
    src/widget.c
    src/ui.c
    src/st.c
    src/st_widget.c
    ${RESOURCE_FILES}
    tags
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

target_link_libraries(tersh vtparse mrsh
    ${BEARLIBTERMINAL_LIB}
)
target_include_directories(tersh PUBLIC
    "${PROJECT_BINARY_DIR}/include"
)

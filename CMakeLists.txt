cmake_minimum_required(VERSION 3.0)

project(install_name_tool)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(SRCS "src/${PROJECT_NAME}.cpp" "src/patchelf.cpp")

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE include)

add_executable(patchelf "src/patchelf.cpp")
target_include_directories(patchelf PRIVATE include)
target_compile_definitions(patchelf PRIVATE PATCHELF)

add_library(test SHARED "src/test.cpp")

# Check RPATH editing.
set(CHECK1_OUT "${CMAKE_BINARY_DIR}/check1.out")
set(CHECK1_REF "${CMAKE_SOURCE_DIR}/src/check1.ref")
add_custom_target(check1 ALL
	COMMAND $<TARGET_FILE:patchelf> --set-rpath \"\" $<TARGET_FILE:test> >${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --set-rpath \"hello:world\" $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:${PROJECT_NAME}> -add_rpath \"hello\" -add_rpath \"helloagain\" $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:${PROJECT_NAME}> -delete_rpath \"world\" $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:${PROJECT_NAME}> -rpath \"hello\" \"helloagain\" $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:${PROJECT_NAME}> -delete_rpath \"helloagain\" $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND $<TARGET_FILE:patchelf> --print-rpath $<TARGET_FILE:test> >>${CHECK1_OUT}
	COMMAND diff ${CHECK1_OUT} ${CHECK1_REF}
)
add_dependencies(check1 patchelf test)

# TODO Check ldd-dependencies editing.


# Created by and for Qt Creator This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

cmake_minimum_required(VERSION 3.16)
project(Milestone4)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
find_package(Threads REQUIRED)
include(CTest)



set (SRCS
    include/aiws/gui/context_widget.hpp
    include/aiws/gui/main_window.hpp
    include/aiws/gui/resettable.hpp
    include/aiws/gui/search_result_model.hpp
    include/aiws/gui/search_widget.hpp
    include/aiws/gui/term_inspector_widget.hpp
    include/aiws/gui/workspace_widget.hpp
    include/aiws/chunker.hpp
    include/aiws/chunking_strategy.hpp
    include/aiws/context_builder.hpp
    include/aiws/context_strategy.hpp
    include/aiws/corpus_index.hpp
    include/aiws/document.hpp
    include/aiws/processing_core.hpp
    include/aiws/processing_types.hpp
    include/aiws/retrieval_engine.hpp
    include/aiws/retrieval_strategy.hpp
    include/aiws/text_processor.hpp
    include/aiws/workspace.hpp
    include/aiws/ipc_server.hpp
    src/chunker.cpp
    src/context_builder.cpp
    src/context_widget.cpp
    src/corpus_index.cpp
    src/document.cpp
    src/main.cpp
    src/main_window.cpp
    src/processing_core.cpp
    src/retrieval_engine.cpp
    src/search_result_model.cpp
    src/search_widget.cpp
    src/term_inspector_widget.cpp
    src/text_processor.cpp
    src/workspace.cpp
    src/workspace_widget.cpp
    src/ipc_server.cpp
)

add_executable(ai_workspace_m4 ${SRCS})

target_include_directories(ai_workspace_m4
    PRIVATE
        include
)

target_link_libraries(ai_workspace_m4 PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Threads::Threads)

if(UNIX)
    add_executable(aiws_send tools/aiws_send.cpp)
endif()

if(BUILD_TESTING)
    set(TEST_SRCS ${SRCS})
    list(REMOVE_ITEM TEST_SRCS src/main.cpp)
    add_executable(student_tests ${TEST_SRCS} tests/student_tests.cpp)
    target_include_directories(student_tests PRIVATE include)
    target_link_libraries(student_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Threads::Threads)
    add_test(NAME student_tests COMMAND student_tests)
    set_tests_properties(student_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 55
    )
    if(UNIX)
        add_dependencies(student_tests aiws_send)
    endif()
endif()

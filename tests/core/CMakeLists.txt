cmake_minimum_required(VERSION 3.21)
project(NkMailCoreTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Sql Test)
find_package(Qt6Keychain REQUIRED)
enable_testing()
add_executable(nkmail_core_tests
    tst_core.cpp
    ../../src/MailTypes.cpp
    ../../src/MessageListModel.cpp ../../src/MessageListModel.h
    ../../src/FolderListModel.cpp ../../src/FolderListModel.h
    ../../src/storage/MailDatabase.cpp ../../src/storage/MailDatabase.h
    ../../src/storage/CredentialStore.cpp ../../src/storage/CredentialStore.h)
target_include_directories(nkmail_core_tests PRIVATE ../../src)
get_filename_component(NKMAIL_TEST_TMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp/core-tests" ABSOLUTE)
target_compile_definitions(nkmail_core_tests PRIVATE NKMAIL_TEST_TMP_ROOT="${NKMAIL_TEST_TMP_ROOT}")
target_link_libraries(nkmail_core_tests PRIVATE Qt6::Core Qt6::Sql Qt6::Test qt6keychain)
target_compile_options(nkmail_core_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME nkmail_core COMMAND nkmail_core_tests)
set_tests_properties(nkmail_core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

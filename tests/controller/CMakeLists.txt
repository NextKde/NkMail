cmake_minimum_required(VERSION 3.21)
project(NkMailControllerTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Sql Test Concurrent DBus Network)
enable_testing()
# Compile the real controller/database/model. Only the blocking protocol and
# credential boundary are synthetic: no mailio, network or system keyring calls.
include(../../cmake/NkMailServices.cmake)
include(../../cmake/NkMailGoogleAuth.cmake)
add_executable(nkmail_controller_tests
    tst_controller.cpp
    ${NKMAIL_SERVICE_SOURCES}
    ../../src/MailController.cpp ../../src/MailController.h
    ../../src/MailTypes.cpp
    ../../src/mail/HtmlSanitizer.cpp
    ../../src/MessageListModel.cpp ../../src/MessageListModel.h
    ../../src/storage/MailDatabase.cpp ../../src/storage/MailDatabase.h
    ../../src/storage/CredentialStore.h)
target_include_directories(nkmail_controller_tests PRIVATE ../../src)
target_link_libraries(nkmail_controller_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Test Qt6::Concurrent Qt6::DBus Qt6::Network nkmail_google_auth)
get_filename_component(NKMAIL_CONTROLLER_TMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../tmp/controller-tests" ABSOLUTE)
target_compile_definitions(nkmail_controller_tests PRIVATE NKMAIL_CONTROLLER_TMP_ROOT="${NKMAIL_CONTROLLER_TMP_ROOT}")
target_compile_options(nkmail_controller_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME nkmail_controller COMMAND nkmail_controller_tests)
set_tests_properties(nkmail_controller PROPERTIES TIMEOUT 30)

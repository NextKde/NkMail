cmake_minimum_required(VERSION 3.30)
project(NkMail VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Sql Concurrent Network)
find_package(Qt6Keychain REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
set(NKMAIL_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nextkdeAppDemo" CACHE PATH "Pinned nextkdeAppDemo source directory (override for local development)")
if(NOT EXISTS "${NKMAIL_UI_SOURCE_DIR}/shared/qml/CMakeLists.txt")
    message(FATAL_ERROR
        "Kos.Ui source is missing at ${NKMAIL_UI_SOURCE_DIR}. "
        "Run 'git submodule update --init --recursive' in the NkMail checkout, "
        "or set NKMAIL_UI_SOURCE_DIR to a valid nextkdeAppDemo checkout for local development.")
endif()
add_subdirectory("${NKMAIL_UI_SOURCE_DIR}/shared/qml" kos-ui)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies statically")
set(MAILIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MAILIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MAILIO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/mailio EXCLUDE_FROM_ALL)
add_library(nkmail_core STATIC
    src/MailTypes.cpp src/MailTypes.h
    src/MessageListModel.cpp src/MessageListModel.h
    src/FolderListModel.cpp src/FolderListModel.h
    src/storage/MailDatabase.cpp src/storage/MailDatabase.h
    src/storage/CredentialStore.cpp src/storage/CredentialStore.h
    src/mail/backend.cpp src/mail/backend.h)
target_include_directories(nkmail_core PUBLIC src)
target_link_libraries(nkmail_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql Qt6Keychain::Qt6Keychain mailio)
qt_add_executable(nkmail src/main.cpp src/MailController.cpp src/MailController.h)
set_source_files_properties(qml/MailStyle.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(nkmail
    URI NextKde.Mail VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/WindowChrome.qml qml/MailAction.qml qml/MailStyle.qml qml/MailComposer.qml qml/AccountDialog.qml)
target_link_libraries(nkmail PRIVATE nkmail_core Kos::Ui Kos::UiPlugin
    Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Concurrent Qt6::Network)
target_compile_definitions(nkmail PRIVATE NKMAIL_VERSION="${PROJECT_VERSION}")
install(TARGETS nkmail RUNTIME DESTINATION bin)
install(FILES data/nkmail.desktop DESTINATION share/applications)
install(FILES data/nkmail.svg DESTINATION share/icons/hicolor/scalable/apps)
option(NKMAIL_BUILD_TESTS "Build isolated local regression tests" OFF)
if(NKMAIL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/core)
    add_subdirectory(tests/controller)
    add_subdirectory(tests/protocol)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME nkmail_cli COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/check_cli.py" $<TARGET_FILE:nkmail>)
    set_tests_properties(nkmail_cli PROPERTIES TIMEOUT 60)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest)
    qt_add_executable(nkmail_ui_tests tests/ui/main.cpp)
    qt_add_qml_module(nkmail_ui_tests
        URI NextKde.Mail.TestViews VERSION 1.0 RESOURCE_PREFIX /qt/qml
        QML_FILES qml/Main.qml qml/WindowChrome.qml qml/MailAction.qml qml/MailStyle.qml qml/MailComposer.qml qml/AccountDialog.qml)
    target_compile_definitions(nkmail_ui_tests PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui")
    target_link_libraries(nkmail_ui_tests PRIVATE Qt6::QuickTest Qt6::QuickControls2 Qt6::QuickDialogs2 Kos::Ui Kos::UiPlugin)
    add_test(NAME nkmail_ui COMMAND nkmail_ui_tests -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui")
    set_tests_properties(nkmail_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1")
endif()

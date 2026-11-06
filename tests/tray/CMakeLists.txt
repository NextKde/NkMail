find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)
add_executable(nkmail_tray_tests tst_tray.cpp
    ../../src/platform/TrayService.cpp ../../src/platform/TrayService.h ../../src/platform/SystemTray.cpp)
target_include_directories(nkmail_tray_tests PRIVATE ../../src)
target_link_libraries(nkmail_tray_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME nkmail_tray COMMAND nkmail_tray_tests)
set_tests_properties(nkmail_tray PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

qt_add_resources(nkmail_tray_tests tray_test_icons PREFIX "/" BASE "${CMAKE_SOURCE_DIR}" FILES "${CMAKE_SOURCE_DIR}/data/nkmail.svg")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Test)
add_executable(nkmail_tray_tests tst_tray.cpp
    ../../src/platform/TrayService.cpp ../../src/platform/TrayService.h)
target_include_directories(nkmail_tray_tests PRIVATE ../../src)
target_link_libraries(nkmail_tray_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME nkmail_tray COMMAND nkmail_tray_tests)
set_tests_properties(nkmail_tray PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

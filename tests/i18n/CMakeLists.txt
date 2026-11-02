find_package(Qt6 REQUIRED COMPONENTS Test Qml Widgets)
add_executable(nkmail_i18n_tests tst_i18n.cpp ../../src/platform/LanguageService.cpp ../../src/platform/LanguageService.h ../../src/platform/TrayService.cpp ../../src/platform/TrayService.h ../../src/platform/SystemTray.cpp)
target_include_directories(nkmail_i18n_tests PRIVATE ../../src)
target_link_libraries(nkmail_i18n_tests PRIVATE Qt6::Test Qt6::Qml Qt6::Widgets)
qt_add_resources(nkmail_i18n_tests test_translations PREFIX "/i18n"
    BASE "${CMAKE_BINARY_DIR}" FILES ${NKMAIL_QM_FILES})
add_test(NAME nkmail_i18n COMMAND nkmail_i18n_tests)

add_test(NAME nkmail_translations COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/check_catalogs.py" $<TARGET_FILE:Qt6::lupdate>)

qt_add_resources(nkmail_i18n_tests test_icons PREFIX "/" BASE "${CMAKE_SOURCE_DIR}" FILES "${CMAKE_SOURCE_DIR}/data/nkmail.svg")
set_tests_properties(nkmail_i18n PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

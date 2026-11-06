find_package(Qt6 REQUIRED COMPONENTS Core Test DBus)
add_executable(nkmail_notification_tests tst_notifications.cpp
    ../../src/NotificationService.cpp ../../src/NotificationService.h
    ../../src/services/MailAlerts.cpp ../../src/services/MailAlerts.h)
target_include_directories(nkmail_notification_tests PRIVATE ../../src)
target_link_libraries(nkmail_notification_tests PRIVATE Qt6::Core Qt6::Test Qt6::DBus)
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(NAME nkmail_notifications COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:nkmail_notification_tests>)
set_tests_properties(nkmail_notifications PROPERTIES TIMEOUT 30)

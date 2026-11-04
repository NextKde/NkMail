find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Test Concurrent)
add_executable(nkmail_google_oauth_tests tst_google_oauth.cpp
    ../../src/auth/GoogleOAuth.cpp ../../src/auth/GoogleOAuth.h)
set_target_properties(nkmail_google_oauth_tests PROPERTIES AUTOMOC ON)
target_compile_definitions(nkmail_google_oauth_tests PRIVATE NKMAIL_OAUTH_TESTING)
target_include_directories(nkmail_google_oauth_tests PRIVATE ../../src)
target_link_libraries(nkmail_google_oauth_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Test Qt6::Concurrent)
add_test(NAME nkmail_google_oauth COMMAND nkmail_google_oauth_tests)
set_tests_properties(nkmail_google_oauth PROPERTIES TIMEOUT 60)

set_source_files_properties(project-client.fixture.json PROPERTIES QT_RESOURCE_ALIAS "google-client.json")
qt_add_resources(nkmail_google_oauth_tests project_client_fixture PREFIX "/oauth" FILES project-client.fixture.json)

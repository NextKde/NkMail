if(NOT TARGET nkmail_google_auth)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Concurrent)
    add_library(nkmail_google_auth STATIC
        ${CMAKE_CURRENT_LIST_DIR}/../src/auth/GoogleOAuth.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../src/auth/GoogleOAuth.h)
    set_target_properties(nkmail_google_auth PROPERTIES AUTOMOC ON)
    target_include_directories(nkmail_google_auth PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../src)
    target_link_libraries(nkmail_google_auth PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent)
endif()

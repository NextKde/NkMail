# Shared source list for the application and isolated controller/service suite.
set(NKMAIL_SERVICE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/AccountSession.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/services/AccountSession.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/MailboxCommands.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/services/MailboxCommands.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/RemoteImageLoader.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/services/RemoteImageLoader.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/DraftService.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/services/DraftService.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/SendService.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/services/SendService.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/MailboxSync.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/services/TaskRunner.h)

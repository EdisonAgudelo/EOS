

if(NOT DEFINED EOS_CONFIG_PATH)
    message(FATAL_ERROR "EOS: Include path for eos_config.h no defined")
endif(NOT DEFINED EOS_CONFIG_PATH)


add_library(EOS
    ${CMAKE_CURRENT_LIST_DIR}/config.h
    ${CMAKE_CURRENT_LIST_DIR}/eos.c
    ${CMAKE_CURRENT_LIST_DIR}/eos.h
    ${CMAKE_CURRENT_LIST_DIR}/list.h
    ${CMAKE_CURRENT_LIST_DIR}/mailbox.h
    ${CMAKE_CURRENT_LIST_DIR}/mailbox.c
    ${CMAKE_CURRENT_LIST_DIR}/scheduler.h
    ${CMAKE_CURRENT_LIST_DIR}/scheduler.c
    ${CMAKE_CURRENT_LIST_DIR}/semaphore.h
    ${CMAKE_CURRENT_LIST_DIR}/semaphore.c
)


target_include_directories(EOS PUBLIC ${EOS_CONFIG_PATH})
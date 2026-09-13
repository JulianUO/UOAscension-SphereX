cmake_minimum_required(VERSION 3.20)
project(sphere_login_crypto C CXX)

add_library(sphere_login_crypto SHARED sphere_login_crypto.cpp)
target_include_directories(sphere_login_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sphere_login_crypto PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
if(WIN32)
    target_compile_definitions(sphere_login_crypto PRIVATE SPHERE_LOGIN_CRYPTO_EXPORTS)
endif()

install(TARGETS sphere_login_crypto
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

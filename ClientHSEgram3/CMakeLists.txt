cmake_minimum_required(VERSION 3.16)

project(ClientHSEgram VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql Multimedia Test)


set(LIBSODIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/libsodium/libsodium-win64/include")
set(LIBSODIUM_LIB         "${CMAKE_CURRENT_SOURCE_DIR}/libs/libsodium/libsodium-win64/lib/libsodium.dll.a")

# ОСССНООООВЫЫЫЫ

qt_add_executable(ClientHSEgram
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    chatwindow.cpp
    chatwindow.h
    chatwindow.ui
    cryptoengine.cpp
    cryptoengine.h
    res.qrc
)

target_include_directories(ClientHSEgram PRIVATE ${LIBSODIUM_INCLUDE_DIR})
target_link_libraries(ClientHSEgram PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Multimedia
    ${LIBSODIUM_LIB}
)

set_target_properties(ClientHSEgram PROPERTIES WIN32_EXECUTABLE TRUE)


# Тесты

enable_testing()

add_executable(MessengerTests
    ctest.cpp
    cryptoengine.cpp
    ctest.h
)

target_include_directories(MessengerTests PRIVATE ${LIBSODIUM_INCLUDE_DIR})
target_link_libraries(MessengerTests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Sql
    ${LIBSODIUM_LIB}
)

add_test(NAME CryptoAndNetworkTest COMMAND MessengerTests)

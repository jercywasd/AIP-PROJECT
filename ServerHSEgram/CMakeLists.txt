cmake_minimum_required(VERSION 3.16)

project(ServerHSEgram LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Sql)

add_executable(ServerHSEgram
  main.cpp
  hseserver.h hseserver.cpp
)
target_include_directories(ServerHSEgram PRIVATE C:/PRACTISE/js/ServerHSEgram/libs/libsodium/libsodium-win64/include)
target_link_libraries(ServerHSEgram PRIVATE Qt6::Core Qt6::Network Qt6::Sql C:/PRACTISE/js/ServerHSEgram/libs/libsodium/libsodium-win64/lib/libsodium.dll.a)

include(GNUInstallDirs)
install(TARGETS ServerHSEgram
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

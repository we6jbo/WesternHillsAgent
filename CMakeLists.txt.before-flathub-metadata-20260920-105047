cmake_minimum_required(VERSION 3.21)

project(WesternHillsAgent VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql)

qt_add_executable(WesternHillsAgent
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    agentcore.cpp
    agentcore.h
    agentidentity.cpp
    agentidentity.h
    portmanager.cpp
    portmanager.h
    agentprotocol.cpp
    agentprotocol.h
    continuationmanager.cpp
    continuationmanager.h
    moltbookconnector.cpp
    moltbookconnector.h
    localagentconnector.cpp
    localagentconnector.h
)

target_link_libraries(WesternHillsAgent PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

install(TARGETS WesternHillsAgent
    RUNTIME DESTINATION bin
)

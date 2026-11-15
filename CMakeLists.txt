cmake_minimum_required(VERSION 3.22)
project(FolderBadges VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(CTest)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons KIO)

# KDE's default settings forbid a few Qt conveniences; keep the code portable instead.
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_KEYWORDS)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

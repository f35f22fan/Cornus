cmake_minimum_required(VERSION 3.10)
Project(Cornus)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
    cmake_policy(SET CMP0100 NEW)
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
ADD_DEFINITIONS(-DQT_NO_KEYWORDS)

if (DEFINED ENV{PREFER_CLANG})
SET (CMAKE_C_COMPILER "/usr/bin/clang-11")
SET (CMAKE_CXX_COMPILER "/usr/bin/clang++-11")
SET (CMAKE_CXX_FLAGS "-Wall")
endif()

find_package(Qt5 COMPONENTS Core Gui Widgets DBus REQUIRED)
include(FindPkgConfig)

find_package(PkgConfig REQUIRED)
# pkg_search_module(DCONF REQUIRED dconf)
pkg_search_module(UDISKS REQUIRED udisks2)
pkg_search_module(UDEV REQUIRED libudev)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "C++ Compiler: " ${CMAKE_CXX_COMPILER})
message(STATUS "Qt5Core: " ${Qt5Core_VERSION})

set(cornus_src_files main.cpp
    App.cpp App.hpp
    AutoDelete.hh
    ByteArray.cpp ByteArray.hpp
    category.cc category.hh
    decl.hxx
    DesktopFile.cpp DesktopFile.hpp
    ElapsedTimer.cpp ElapsedTimer.hpp
    err.hpp
    ExecInfo.cpp ExecInfo.hpp
    History.cpp History.hpp
    Media.hpp Media.cpp media.hxx
    MutexGuard.cpp MutexGuard.hpp
    prefs.cc prefs.hh
    Prefs.cpp Prefs.hpp
    TreeData.cpp TreeData.hpp

    gui/actions.hxx
    gui/AttrsDialog.cpp gui/AttrsDialog.hpp
    gui/CompleterModel.cpp gui/CompleterModel.hpp
    gui/CountFolder.cpp gui/CountFolder.hpp
    gui/decl.hxx
    gui/Hiliter.cpp gui/Hiliter.hpp
    gui/Location.cpp gui/Location.hpp
    gui/MediaDialog.cpp gui/MediaDialog.hpp
    gui/OpenOrderPane.cpp gui/OpenOrderPane.hpp
    gui/OpenOrderModel.cpp gui/OpenOrderModel.hpp
    gui/OpenOrderTable.cpp gui/OpenOrderTable.hpp
    gui/OpenWithPane.cpp gui/OpenWithPane.hpp
    gui/PrefsPane.cpp gui/PrefsPane.hpp
    gui/RestorePainter.hpp
    gui/SearchLineEdit.cpp gui/SearchLineEdit.hpp
    gui/SearchPane.cpp gui/SearchPane.hpp
    gui/sidepane.cc gui/sidepane.hh
    gui/Table.cpp gui/Table.hpp
    gui/TableDelegate.cpp gui/TableDelegate.hpp
    gui/TableHeader.cpp gui/TableHeader.hpp
    gui/TableModel.cpp gui/TableModel.hpp
    gui/TextEdit.cpp gui/TextEdit.hpp
    gui/TextField.cpp gui/TextField.hpp
    gui/ToolBar.cpp gui/ToolBar.hpp
    gui/TreeItem.cpp gui/TreeItem.hpp
    gui/TreeModel.cpp gui/TreeModel.hpp
    gui/TreeView.cpp gui/TreeView.hpp
    
    io/decl.hxx
    io/DirStream.cpp io/DirStream.hpp
    io/disks.cc io/disks.hh
    io/File.cpp io/File.hpp
    io/io.cc io/io.hh
    io/socket.cc io/socket.hh
)

foreach(f IN LISTS cornus_src_files)
	get_filename_component(b ${f} NAME)
	set_source_files_properties(${f} PROPERTIES
		COMPILE_DEFINITIONS "SRC_FILE_NAME=\"${b}\"")
endforeach()

set(cornus_io_src_files main_io.cpp
    AutoDelete.hh
    category.cc category.hh
    ByteArray.cpp ByteArray.hpp
    DesktopFile.cpp DesktopFile.hpp
    ElapsedTimer.cpp ElapsedTimer.hpp
    err.hpp
    MutexGuard.cpp MutexGuard.hpp
    MyDaemon.cpp MyDaemon.hpp
    prefs.cc prefs.hh

    io/decl.hxx
    io/File.cpp io/File.hpp
    io/io.cc io/io.hh
    io/Server.cpp io/Server.hpp
    io/socket.cc io/socket.hh
    io/Task.cpp io/Task.hpp

    gui/TaskGui.cpp gui/TaskGui.hpp
    gui/TasksWin.cpp gui/TasksWin.hpp
)

foreach(f IN LISTS cornus_io_src_files)
	get_filename_component(b ${f} NAME)
	set_source_files_properties(${f} PROPERTIES
		COMPILE_DEFINITIONS "SRC_FILE_NAME=\"${b}\"")
endforeach()
   
set (cornus_exe "cornus")
add_executable(${cornus_exe} ${cornus_src_files} resources/resources.qrc)
target_include_directories(${cornus_exe} PRIVATE
    ${UDISKS_INCLUDE_DIRS} ${UDEV_INCLUDE_DIRS}) # ${DCONF_INCLUDE_DIRS}
target_link_libraries(${cornus_exe}  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus
    ${UDISKS_LDFLAGS} ${UDEV_LDFLAGS} pthread) # ${DCONF_LDFLAGS}
   
set (cornus_io_exe "cornus_io")
add_executable(${cornus_io_exe} ${cornus_io_src_files} resources/resources.qrc)
target_include_directories(${cornus_io_exe} PRIVATE
    ${UDISKS_INCLUDE_DIRS}) # ${DCONF_INCLUDE_DIRS}
target_link_libraries(${cornus_io_exe}  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus
    ${UDISKS_LDFLAGS} pthread) # ${DCONF_LDFLAGS}


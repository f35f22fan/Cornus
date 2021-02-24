cmake_minimum_required(VERSION 3.10)
Project(Cornus)
#cmake_policy(SET CMP0100 NEW)

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 COMPONENTS Core Gui Widgets DBus REQUIRED)
include(FindPkgConfig)

find_package(PkgConfig REQUIRED)
pkg_search_module(DCONF REQUIRED dconf)
pkg_search_module(UDISKS REQUIRED udisks2)
#libdbus-c++-dev

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "C++ Compiler: " ${CMAKE_CXX_COMPILER})
message(STATUS "Qt5Core: " ${Qt5Core_VERSION})

set(cornus_src_files main.cpp
    App.cpp App.hpp
    AutoDelete.hh
    ByteArray.cpp ByteArray.hpp
    decl.hxx
    defines.hxx
    DesktopFile.cpp DesktopFile.hpp
    ElapsedTimer.cpp ElapsedTimer.hpp
    err.hpp
    ExecInfo.cpp ExecInfo.hpp
    History.cpp History.hpp
    MutexGuard.cpp MutexGuard.hpp
    prefs.cc prefs.hh
    Prefs.cpp Prefs.hpp
    
    gui/actions.hxx
    gui/CountFolder.cpp gui/CountFolder.hpp
    gui/decl.hxx
    gui/Hiliter.cpp gui/Hiliter.hpp
    gui/Location.cpp gui/Location.hpp
    gui/OpenOrderPane.cpp gui/OpenOrderPane.hpp
    gui/OpenOrderModel.cpp gui/OpenOrderModel.hpp
    gui/OpenOrderTable.cpp gui/OpenOrderTable.hpp
    gui/OpenWithPane.cpp gui/OpenWithPane.hpp
    gui/PrefsPane.cpp gui/PrefsPane.hpp
    gui/SearchPane.cpp gui/SearchPane.hpp
    gui/SidePane.cpp gui/SidePane.hpp
    gui/SidePaneItem.cpp gui/SidePaneItem.hpp
    gui/SidePaneModel.cpp gui/SidePaneModel.hpp
    gui/Table.cpp gui/Table.hpp
    gui/TableDelegate.cpp gui/TableDelegate.hpp
    gui/TableModel.cpp gui/TableModel.hpp
    gui/TextEdit.cpp gui/TextEdit.hpp
    gui/ToolBar.cpp gui/ToolBar.hpp
    
    io/decl.hxx
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
    defines.hxx
    err.hpp
    ByteArray.cpp ByteArray.hpp
    DesktopFile.cpp DesktopFile.hpp
    ElapsedTimer.cpp ElapsedTimer.hpp
    MutexGuard.cpp MutexGuard.hpp
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
add_executable(${cornus_exe} ${cornus_src_files} resources.qrc)
target_include_directories(${cornus_exe} PRIVATE
    ${UDISKS_INCLUDE_DIRS} ${DCONF_INCLUDE_DIRS})
target_link_libraries(${cornus_exe}  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus
    ${UDISKS_LDFLAGS} ${DCONF_LDFLAGS} pthread)
   
set (cornus_io_exe "cornus_io")
add_executable(${cornus_io_exe} ${cornus_io_src_files} resources.qrc)
target_include_directories(${cornus_io_exe} PRIVATE
    ${UDISKS_INCLUDE_DIRS} ${DCONF_INCLUDE_DIRS})
target_link_libraries(${cornus_io_exe}  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus
    ${UDISKS_LDFLAGS} ${DCONF_LDFLAGS} pthread)   


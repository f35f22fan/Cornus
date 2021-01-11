cmake_minimum_required(VERSION 3.10)
Project(Cornus)
cmake_policy(SET CMP0100 NEW)

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
include(FindPkgConfig)

find_package(PkgConfig REQUIRED)
pkg_search_module(DCONF REQUIRED dconf)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "C++ Compiler: " ${CMAKE_CXX_COMPILER})
message(STATUS "Qt5Core: " ${Qt5Core_VERSION} ", Gui: "
 ${Qt5Gui_VERSION} ", Widgets: " ${Qt5Widgets_VERSION})

set(src_files main.cpp
    App.cpp App.hpp
    AutoDelete.hh
    ByteArray.cpp ByteArray.hpp
    decl.hxx
    err.hpp
    History.cpp History.hpp
    MutexGuard.cpp MutexGuard.hpp
    
    gui/decl.hxx
    gui/Location.cpp gui/Location.hpp
    gui/Table.cpp gui/Table.hpp
    gui/TableDelegate.cpp gui/TableDelegate.hpp
    gui/TableModel.cpp gui/TableModel.hpp
    gui/ToolBar.cpp gui/ToolBar.hpp
    
    io/decl.hxx
    io/File.cpp io/File.hpp
    io/io.cc io/io.hh
)
   
foreach(f IN LISTS src_files)
	get_filename_component(b ${f} NAME)
	set_source_files_properties(${f} PROPERTIES
		COMPILE_DEFINITIONS "SRC_FILE_NAME=\"${b}\"")
endforeach()
   
set (exe_name "cornus")
add_executable(${exe_name} ${src_files} resources.qrc)
target_include_directories(${exe_name} PRIVATE ${DCONF_INCLUDE_DIRS})
target_link_libraries(${exe_name}  Qt5::Core Qt5::Gui Qt5::Widgets ${DCONF_LDFLAGS} pthread)

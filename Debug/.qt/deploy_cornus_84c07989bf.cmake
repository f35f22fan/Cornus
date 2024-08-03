include(/home/fox/dev/Cornus/Debug/.qt/QtDeploySupport.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/cornus-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_ALL_MODULES_FOUND_VIA_FIND_PACKAGE "Core;Core5Compat;DBus;Gui;Widgets;Test")

qt6_deploy_runtime_dependencies(
    EXECUTABLE /home/fox/dev/Cornus/Debug/cornus
    GENERATE_QT_CONF
)

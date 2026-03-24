cmake_minimum_required(VERSION 3.16)

project(MusicPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt 组件
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia)

# 将所有源文件列出
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    marqueelabel.h
    marqueelabel.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MusicPlayer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        res.qrc
        songitemdelegate.h
        musicplaylist.h musicplaylist.cpp musicplaylist.ui
        mediaplayerpool.h mediaplayerpool.cpp
        lrcparser.h lrcparser.cpp
        playercontroller.h playercontroller.cpp
        moremenu.h moremenu.cpp moremenu.ui
        playliststore.h playliststore.cpp
    )
else()
    if(ANDROID)
        add_library(MusicPlayer SHARED ${PROJECT_SOURCES})
    else()
        add_executable(MusicPlayer ${PROJECT_SOURCES})
    endif()
endif()

# 链接 Qt 库
target_link_libraries(MusicPlayer PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
)

# 添加当前源目录到包含路径
target_include_directories(MusicPlayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ========== 新增：根据构建类型设置编译定义 ==========
# Release 模式下禁用 qDebug / qWarning 输出
target_compile_definitions(MusicPlayer PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
    $<$<CONFIG:Release>:QT_NO_WARNING_OUTPUT>
)
# ==================================================

# 设置 macOS/iOS 相关属性
if(QT_VERSION VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MusicPlayer)
else()
  set(BUNDLE_ID_OPTION)
endif()
set_target_properties(MusicPlayer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

# ========== 安装与打包配置 ==========

# 基础安装前缀设为 /opt/MusicPlayer
set(BASE_INST_DIR "opt/MusicPlayer")

# 1. 安装可执行文件到 /opt/MusicPlayer/bin
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
install(TARGETS MusicPlayer
    RUNTIME DESTINATION "${BASE_INST_DIR}/bin"
)

# 2. 安装背景图片
install(FILES 
    "res/background.png"
    "res/background_dark.png"
    "res/background_fade.png"
    "res/background_fade2.png"
    DESTINATION "${BASE_INST_DIR}/share/backgrounds"
)

# 3. 安装图标 (用于 .desktop 文件，使用绝对路径确保系统识别)
install(FILES "res/misaka.png"
    DESTINATION "usr/share/icons/hicolor/256x256/apps"
    RENAME "MusicPlayer.png"
)

# 4. 安装 .desktop 文件到系统应用目录 (使用绝对路径确保系统识别)
install(FILES "MusicPlayer.desktop"
    DESTINATION "usr/share/applications"
)

# ========== 运行时依赖与插件打包逻辑 ==========

if(QT_VERSION_MAJOR EQUAL 6)
    get_target_property(_qt_library_path Qt6::Core LOCATION)
else()
    get_target_property(_qt_library_path Qt5::Core LOCATION)
endif()
get_filename_component(_qt_bin_dir "${_qt_library_path}" DIRECTORY)
get_filename_component(_qt_root_dir "${_qt_bin_dir}" DIRECTORY)
set(QT_PLUGINS_DIR_REAL "${_qt_root_dir}/plugins")

install(CODE "
    set(QT_PLUGINS_DIR \"${QT_PLUGINS_DIR_REAL}\")
    set(BASE_LIB_DIR \"${BASE_INST_DIR}/lib\")
    set(BASE_PLUGINS_DIR \"${BASE_INST_DIR}/plugins\")
    
    # 1. 获取并安装运行时依赖库
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES \"$<TARGET_FILE:MusicPlayer>\"
        RESOLVED_DEPENDENCIES_VAR _resolved_deps
        UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
        PRE_EXCLUDE_REGEXES 
            \"libc\\\\.so.*\" \"libgcc_s\\\\.so.*\" \"libm\\\\.so.*\" \"libpthread\\\\.so.*\" \"libstdc\\\\+\\\\+\\\\.so.*\" 
            \"libdl\\\\.so.*\" \"librt\\\\.so.*\" \"libutil\\\\.so.*\" \"libX11\\\\.so.*\" \"libxcb\\\\.so.*\" 
            \"libglib-2\\\\.0\\\\.so.*\" \"libz\\\\.so.*\" \"libsystemd\\\\.so.*\" \"libasound\\\\.so.*\"
            \"libssl\\\\.so.*\" \"libcrypto\\\\.so.*\" \"libfontconfig\\\\.so.*\" \"libfreetype\\\\.so.*\"
            \"libdbus-1\\\\.so.*\" \"libgpg-error\\\\.so.*\" \"libgcrypt\\\\.so.*\" \"liblzma\\\\.so.*\"
            \"libxml2\\\\.so.*\" \"libffi\\\\.so.*\" \"libzstd\\\\.so.*\" \"libcap\\\\.so.*\"
    )
    
    foreach(_file \${_resolved_deps})
        file(INSTALL
            DESTINATION \"\${CMAKE_INSTALL_PREFIX}/\${BASE_LIB_DIR}\"
            TYPE SHARED_LIBRARY
            FILES \"\${_file}\"
            FOLLOW_SYMLINK_CHAIN
        )
    endforeach()

    # 2. 打包 Qt 插件
    set(_plugins_to_copy \"platforms\" \"multimedia\" \"imageformats\")
    foreach(_plugin \${_plugins_to_copy})
        if(EXISTS \"\${QT_PLUGINS_DIR}/\${_plugin}\")
            file(INSTALL
                DESTINATION \"\${CMAKE_INSTALL_PREFIX}/\${BASE_PLUGINS_DIR}\"
                TYPE DIRECTORY
                FILES \"\${QT_PLUGINS_DIR}/\${_plugin}\"
            )
        endif()
    endforeach()
")

# ========== CPack 配置 (用于生成 RPM 包) ==========
set(CPACK_PACKAGE_NAME "MusicPlayer")
set(CPACK_PACKAGE_VENDOR "misaka")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple local music player built with Qt6")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGING_INSTALL_PREFIX "/") # 设为根目录，让 BASE_INST_DIR 有效

# RPM 专用设置
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_AUTOREQ "YES") 

# 核心修正：排除所有系统级共享目录
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION 
    "/opt" "/usr" "/usr/share" "/usr/share/applications" "/usr/share/icons" "/usr/share/icons/hicolor" "/usr/share/icons/hicolor/256x256" "/usr/share/icons/hicolor/256x256/apps"
)

include(CPack)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MusicPlayer)
endif()

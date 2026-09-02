cmake_minimum_required(VERSION 3.19)

# ---------------------------------------------------------------------------
#  Tuấn' MultiTele Client
#  Trình khách Telegram đa tài khoản, viết bằng C++/Qt 6.
# ---------------------------------------------------------------------------

# Phiên bản được đọc từ file VERSION ở gốc dự án. CI (hoặc git hook) tự tăng
# số patch mỗi khi mã nguồn thay đổi, nên mọi bản build đều có phiên bản riêng.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _APP_VERSION_RAW)
string(STRIP "${_APP_VERSION_RAW}" _APP_VERSION_FILE)

set(APP_VERSION "${_APP_VERSION_FILE}" CACHE STRING "Phiên bản ứng dụng (ghi đè được từ CI)")

if(NOT APP_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "APP_VERSION khong hop le: '${APP_VERSION}' (can dang MAJOR.MINOR.PATCH)")
endif()

project(TuanMultiTeleClient
    VERSION ${APP_VERSION}
    DESCRIPTION "Trinh khach Telegram da tai khoan"
    HOMEPAGE_URL "https://github.com/tlearnvn/multi-telegram-manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Kiểu build" FORCE)
endif()

# Gom mọi thứ vào một thư mục để đóng gói bản portable cho gọn. Với generator
# nhiều cấu hình (Visual Studio) phải đặt riêng từng cấu hình, nếu không MSVC sẽ
# thêm thư mục con Release/Debug.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

find_package(Qt6 6.3 REQUIRED COMPONENTS Core Gui Widgets)

# --- Thông tin build được nhúng vào nhị phân -------------------------------
include(cmake/GitInfo.cmake)
tuan_get_git_info(APP_GIT_COMMIT APP_GIT_BUILD_NUMBER)

string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d" UTC)

set(APP_NAME "Tuấn' MultiTele Client")
set(APP_ORGANIZATION "tlearnvn")
set(APP_EXECUTABLE_NAME "TuanMultiTeleClient")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
    @ONLY)

message(STATUS "Tuan' MultiTele Client ${APP_VERSION} (build ${APP_GIT_BUILD_NUMBER}, ${APP_GIT_COMMIT})")

# --- Danh sách nguồn -------------------------------------------------------
# Mọi thứ trừ main.cpp nằm trong một thư viện tĩnh, nhờ vậy công cụ xem trước
# giao diện (tools/uipreview.cpp) dùng lại được cùng mã nguồn.
set(APP_SOURCES
    src/core/apppaths.cpp
    src/core/apppaths.h
    src/core/logging.cpp
    src/core/logging.h
    src/core/settingsstore.cpp
    src/core/settingsstore.h
    src/core/jsonutil.cpp
    src/core/jsonutil.h
    src/core/formatting.cpp
    src/core/formatting.h
    src/core/qrcode.cpp
    src/core/qrcode.h

    src/td/tdloader.cpp
    src/td/tdloader.h
    src/td/tdtransport.cpp
    src/td/tdtransport.h
    src/td/tdaccount.cpp
    src/td/tdaccount.h
    src/td/tdenums.h
    src/td/accountmanager.cpp
    src/td/accountmanager.h
    src/td/filecache.cpp
    src/td/filecache.h

    src/model/chatentry.h
    src/model/messageentry.h
    src/model/userentry.h
    src/model/chatlistmodel.cpp
    src/model/chatlistmodel.h
    src/model/messagemodel.cpp
    src/model/messagemodel.h

    src/ui/theme.cpp
    src/ui/theme.h
    src/ui/iconfactory.cpp
    src/ui/iconfactory.h
    src/ui/avatarpainter.cpp
    src/ui/avatarpainter.h
    src/ui/flatbutton.cpp
    src/ui/flatbutton.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/accountrail.cpp
    src/ui/accountrail.h
    src/ui/chatlistpane.cpp
    src/ui/chatlistpane.h
    src/ui/chatlistdelegate.cpp
    src/ui/chatlistdelegate.h
    src/ui/chatview.cpp
    src/ui/chatview.h
    src/ui/messagedelegate.cpp
    src/ui/messagedelegate.h
    src/ui/composer.cpp
    src/ui/composer.h
    src/ui/emojipicker.cpp
    src/ui/emojipicker.h
    src/ui/logindialog.cpp
    src/ui/logindialog.h
    src/ui/settingsdialog.cpp
    src/ui/settingsdialog.h
    src/ui/chatinfopane.cpp
    src/ui/chatinfopane.h
    src/ui/newchatdialog.cpp
    src/ui/newchatdialog.h
    src/ui/chatpickerdialog.cpp
    src/ui/chatpickerdialog.h
    src/ui/broadcastdialog.cpp
    src/ui/broadcastdialog.h
    src/ui/dashboardpane.cpp
    src/ui/dashboardpane.h
    src/ui/qrview.cpp
    src/ui/qrview.h
    src/ui/trayicon.cpp
    src/ui/trayicon.h
    src/ui/toast.cpp
    src/ui/toast.h
    src/ui/setupwizard.cpp
    src/ui/setupwizard.h
    src/ui/aboutdialog.cpp
    src/ui/aboutdialog.h
)

# --- Thư viện dùng chung ---------------------------------------------------
add_library(tuan_core STATIC ${APP_SOURCES})

target_include_directories(tuan_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(tuan_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

if(MSVC)
    target_compile_options(tuan_core PUBLIC /utf-8 /W3 /permissive-)
    target_compile_definitions(tuan_core PUBLIC
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(tuan_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_compile_definitions(tuan_core PUBLIC
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    QT_NO_CAST_FROM_BYTEARRAY)

set_target_properties(tuan_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Tệp chạy chính -------------------------------------------------------
set(APP_MAIN_SOURCES src/main.cpp)

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc"
        @ONLY)
    list(APPEND APP_MAIN_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc")
endif()

qt_add_executable(${APP_EXECUTABLE_NAME} WIN32 MACOSX_BUNDLE ${APP_MAIN_SOURCES})
target_link_libraries(${APP_EXECUTABLE_NAME} PRIVATE tuan_core)

if(UNIX AND NOT APPLE)
    # Bản portable: tìm thư viện Qt trong ./lib cạnh file chạy.
    set_target_properties(${APP_EXECUTABLE_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib;$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# --- Công cụ xem trước giao diện (không đóng gói kèm) ---------------------
# Dựng cửa sổ với dữ liệu mẫu rồi xuất ảnh PNG, dùng để soi giao diện mà không
# cần tài khoản Telegram thật. Bật bằng -DBUILD_UI_PREVIEW=ON.
option(BUILD_UI_PREVIEW "Build công cụ xuất ảnh xem trước giao diện" OFF)

if(BUILD_UI_PREVIEW)
    add_executable(tuan_uipreview tools/uipreview.cpp)
    target_link_libraries(tuan_uipreview PRIVATE tuan_core)
    if(UNIX AND NOT APPLE)
        set_target_properties(tuan_uipreview PROPERTIES
            INSTALL_RPATH "$ORIGIN/lib"
            BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
endif()

# --- Cài đặt / đóng gói ----------------------------------------------------
include(GNUInstallDirs)

install(TARGETS ${APP_EXECUTABLE_NAME}
    RUNTIME DESTINATION .
    BUNDLE  DESTINATION .)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    DESTINATION .)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/HUONG-DAN.md"
    DESTINATION .
    RENAME "HUONG-DAN.md")

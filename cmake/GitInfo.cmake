# Lấy thông tin git để nhúng vào nhị phân.
#
# tuan_get_git_info(<biến_commit> <biến_số_build>)
#
#  * biến_commit    -> hash ngắn của HEAD, hoặc "nogit" nếu không có git.
#  * biến_số_build  -> số commit tính từ đầu lịch sử; nhờ vậy mỗi commit mới
#                      là một số build mới, phiên bản tự tăng theo mã nguồn.

function(tuan_get_git_info out_commit out_build)
    set(_commit "nogit")
    set(_build "0")

    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _sha_rc)
        if(_sha_rc EQUAL 0 AND _sha)
            set(_commit "${_sha}")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _count
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _count_rc)
        if(_count_rc EQUAL 0 AND _count)
            set(_build "${_count}")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_dirty)
            set(_commit "${_commit}-dirty")
        endif()
    endif()

    if(DEFINED ENV{APP_BUILD_NUMBER} AND NOT "$ENV{APP_BUILD_NUMBER}" STREQUAL "")
        set(_build "$ENV{APP_BUILD_NUMBER}")
    endif()

    set(${out_commit} "${_commit}" PARENT_SCOPE)
    set(${out_build} "${_build}" PARENT_SCOPE)
endfunction()

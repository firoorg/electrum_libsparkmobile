cmake_minimum_required(VERSION 3.10)

project(electrum_libsparkmobile_library VERSION 0.0.1)

if(BUILD_FOR_SYSTEM_NAME STREQUAL "linux")
    ADD_DEFINITIONS(-fPIC)
endif()

if(BUILD_FOR_SYSTEM_NAME STREQUAL "macos" OR BUILD_FOR_SYSTEM_NAME STREQUAL "windows")
    set(CMAKE_CXX_STANDARD 17)
endif()

if(BUILD_FOR_SYSTEM_NAME STREQUAL "macos")
    add_compile_definitions(_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
    SET(SPARK_MISSING_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/missing_headers/darwin")
elseif(BUILD_FOR_SYSTEM_NAME STREQUAL "windows")
    SET(SPARK_MISSING_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/missing_headers/windows")
endif()

add_library(electrum_libsparkmobile SHARED
    "electrum_libsparkmobile.cpp"
    "utils.cpp"
    "transaction.cpp"
)

set(LIBSPARK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SPARKMOBILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sparkmobile")
set(OPENSSL_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl-cmake")

set(SPARKMOBILE_GIT_URL "https://github.com/firoorg/sparkmobile.git")
set(SPARKMOBILE_GIT_COMMIT "a9d078955312b3467f5bb14cbd64ab2ed8852256")
set(OPENSSL_CMAKE_GIT_URL "https://github.com/janbar/openssl-cmake.git")
set(OPENSSL_CMAKE_GIT_COMMIT "b0ac69581958cd658364147da9057da89a01c394")

set(SPARKMOBILE_PATCHED_FILES "CMakeLists.txt" "secp256k1/CMakeLists.txt")
set(OPENSSL_CMAKE_PATCHED_FILES "CMakeLists.txt")

function(pinned_dependency_state DIR COMMIT PATCHED OUT_STATE)
    set(${OUT_STATE} "missing" PARENT_SCOPE)
    if(NOT EXISTS "${DIR}/.git")
        return()
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${DIR}" rev-parse HEAD
        OUTPUT_VARIABLE HEAD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE REV_PARSE_RESULT
        ERROR_QUIET
    )
    if(NOT REV_PARSE_RESULT EQUAL 0 OR NOT HEAD_COMMIT STREQUAL COMMIT)
        set(${OUT_STATE} "wrong-commit" PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${DIR}" status --porcelain --untracked-files=all
        OUTPUT_VARIABLE TREE_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE STATUS_RESULT
        ERROR_QUIET
    )
    if(NOT STATUS_RESULT EQUAL 0)
        set(${OUT_STATE} "wrong-commit" PARENT_SCOPE)
        return()
    endif()
    string(REPLACE "\n" ";" STATUS_LINES "${TREE_STATUS}")
    foreach(LINE IN LISTS STATUS_LINES)
        string(STRIP "${LINE}" LINE)
        if(LINE STREQUAL "")
            continue()
        endif()
        set(LINE_ALLOWED FALSE)
        foreach(ALLOWED IN LISTS PATCHED)
            if(LINE MATCHES " ${ALLOWED}$")
                set(LINE_ALLOWED TRUE)
            endif()
        endforeach()
        if(NOT LINE_ALLOWED)
            message(STATUS "Dependency ${DIR} has unexpected modification: ${LINE}")
            set(${OUT_STATE} "modified" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${OUT_STATE} "ok" PARENT_SCOPE)
endfunction()

function(fetch_pinned_dependency NAME URL COMMIT DIR PATCHED)
    pinned_dependency_state("${DIR}" "${COMMIT}" "${PATCHED}" DEP_STATE)
    if(DEP_STATE STREQUAL "ok")
        message(STATUS "${NAME}: using pinned checkout ${COMMIT}")
        return()
    endif()

    message(STATUS "${NAME}: fetching pinned commit ${COMMIT} (state: ${DEP_STATE})")
    file(REMOVE_RECURSE "${DIR}")
    file(MAKE_DIRECTORY "${DIR}")

    execute_process(COMMAND ${GIT_EXECUTABLE} -C "${DIR}" init --quiet
                    RESULT_VARIABLE INIT_RESULT)
    if(NOT INIT_RESULT EQUAL 0)
        message(FATAL_ERROR "${NAME}: git init failed with ${INIT_RESULT}")
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} -C "${DIR}" remote add origin "${URL}"
                    RESULT_VARIABLE REMOTE_RESULT)
    if(NOT REMOTE_RESULT EQUAL 0)
        message(FATAL_ERROR "${NAME}: git remote add failed with ${REMOTE_RESULT}")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${DIR}" fetch --quiet --depth 1 origin "${COMMIT}"
        RESULT_VARIABLE FETCH_RESULT
        ERROR_QUIET
    )
    if(FETCH_RESULT EQUAL 0)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C "${DIR}" checkout --quiet FETCH_HEAD
            RESULT_VARIABLE CHECKOUT_RESULT
        )
    else()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C "${DIR}" fetch --quiet origin
            RESULT_VARIABLE FULL_FETCH_RESULT
        )
        if(NOT FULL_FETCH_RESULT EQUAL 0)
            message(FATAL_ERROR "${NAME}: git fetch failed with ${FULL_FETCH_RESULT}")
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C "${DIR}" checkout --quiet "${COMMIT}"
            RESULT_VARIABLE CHECKOUT_RESULT
        )
    endif()
    if(NOT CHECKOUT_RESULT EQUAL 0)
        message(FATAL_ERROR "${NAME}: could not check out pinned commit ${COMMIT}")
    endif()

    pinned_dependency_state("${DIR}" "${COMMIT}" "${PATCHED}" DEP_STATE)
    if(NOT DEP_STATE STREQUAL "ok")
        message(FATAL_ERROR
            "${NAME}: checkout does not match pinned commit ${COMMIT} (state: ${DEP_STATE})")
    endif()
endfunction()

find_package(Git REQUIRED)

set(SPARKMOBILE_GIT_REF "" CACHE STRING
    "Upstream branch to follow instead of SPARKMOBILE_GIT_COMMIT")

if(NOT SPARKMOBILE_GIT_REF STREQUAL "")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} ls-remote "${SPARKMOBILE_GIT_URL}" "${SPARKMOBILE_GIT_REF}"
        OUTPUT_VARIABLE SPARKMOBILE_LS_REMOTE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE SPARKMOBILE_LS_REMOTE_RESULT
    )
    if(NOT SPARKMOBILE_LS_REMOTE_RESULT EQUAL 0 OR SPARKMOBILE_LS_REMOTE STREQUAL "")
        message(FATAL_ERROR
            "sparkmobile: could not resolve ref '${SPARKMOBILE_GIT_REF}'")
    endif()
    string(SUBSTRING "${SPARKMOBILE_LS_REMOTE}" 0 40 SPARKMOBILE_GIT_COMMIT)
    message(WARNING
        "sparkmobile: following '${SPARKMOBILE_GIT_REF}', resolved to "
        "${SPARKMOBILE_GIT_COMMIT}. This build is not reproducible; pin this "
        "commit in src/CMakeLists.txt and DEPENDENCIES.md before releasing.")
endif()

fetch_pinned_dependency("sparkmobile" "${SPARKMOBILE_GIT_URL}" "${SPARKMOBILE_GIT_COMMIT}"
                        "${SPARKMOBILE_DIR}" "${SPARKMOBILE_PATCHED_FILES}")
fetch_pinned_dependency("openssl-cmake" "${OPENSSL_CMAKE_GIT_URL}" "${OPENSSL_CMAKE_GIT_COMMIT}"
                        "${OPENSSL_CMAKE_DIR}" "${OPENSSL_CMAKE_PATCHED_FILES}")

file(COPY "${LIBSPARK_ROOT}/patches/sparkmobile/CMakeLists.txt"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/deps/sparkmobile")
file(COPY "${LIBSPARK_ROOT}/patches/sparkmobile/secp256k1/CMakeLists.txt"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/deps/sparkmobile/secp256k1")
file(COPY "${LIBSPARK_ROOT}/patches/openssl-cmake/CMakeLists.txt"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl-cmake")

set(BOOST_URL "https://archives.boost.io/release/1.71.0/source/boost_1_71_0.zip" CACHE STRING "Boost download URL")
set(BOOST_URL_SHA256 "85a94ac71c28e59cf97a96714e4c58a18550c227ac8b0388c260d6c717e47a69" CACHE STRING "Boost download URL SHA256 checksum")
add_subdirectory("deps/boost-cmake")

add_subdirectory("deps/openssl-cmake" EXCLUDE_FROM_ALL)

add_subdirectory("deps/sparkmobile")

set_target_properties(electrum_libsparkmobile PROPERTIES
    PUBLIC_HEADER electrum_libsparkmobile.h
    OUTPUT_NAME "electrum_libsparkmobile"
)

target_link_libraries(electrum_libsparkmobile sparkmobile)

if(NOT MSVC)
    target_compile_options(electrum_libsparkmobile PRIVATE
        -fstack-protector-strong
        -fvisibility=hidden
        -fvisibility-inlines-hidden
    )
    if(BUILD_FOR_SYSTEM_NAME STREQUAL "linux")
        target_link_options(electrum_libsparkmobile PRIVATE
            "-Wl,-z,relro" "-Wl,-z,now" "-Wl,--as-needed"
            "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map")
    elseif(BUILD_FOR_SYSTEM_NAME STREQUAL "macos")
        target_link_options(electrum_libsparkmobile PRIVATE
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/exports.symbols")
    endif()
endif()


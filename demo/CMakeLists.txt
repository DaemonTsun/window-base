find_package(better REQUIRED NO_DEFAULT_PATH PATHS ../ext/better-cmake/cmake/)

add_exe(demo_window 
    VERSION 1.0.0
    SOURCES_DIR "${ROOT}/src"
    CPP_VERSION 20
    CPP_WARNINGS ALL SANE FATAL

    COMPILE_DEFINITIONS ${fs_COMPILE_DEFINITIONS}

    LIBRARIES ${window-base_TARGET}
    INCLUDE_DIRS ${window-base_INCLUDE_DIRS}
                 ${window-base_SOURCES_DIR}
)

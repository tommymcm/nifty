include(GNUInstallDirs)

function(generate_enable_file TEMPLATE)
    set(_template "${CMAKE_CURRENT_FUNCTION_FILE_DIR}/enable.in")

    # --- Build tree ---
    set(ENABLE_BIN_DIR "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
    set(ENABLE_LIB_DIR "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
    set(ENABLE_PREFIX_DIR "${CMAKE_BINARY_DIR}")
    configure_file("${TEMPLATE}" "${CMAKE_BINARY_DIR}/enable" @ONLY)

    # --- Install tree ---
    set(ENABLE_BIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
    set(ENABLE_LIB_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    set(ENABLE_PREFIX_DIR "${CMAKE_INSTALL_PREFIX}")
    configure_file("${TEMPLATE}" "${CMAKE_BINARY_DIR}/.enable_install" @ONLY)

    install(
        FILES "${CMAKE_BINARY_DIR}/.enable_install"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        RENAME "enable-nifty"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
endfunction()

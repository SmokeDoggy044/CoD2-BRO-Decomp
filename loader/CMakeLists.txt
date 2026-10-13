# Route-B only: deleted once the game no longer needs the original XBE.
add_executable(cod_bro_loader
    crash.cpp
    hooks.cpp
    kernel_thunks.cpp
    main.cpp
    patches.cpp
    symbols.cpp
    thread_env.cpp
    xbe.cpp
)
target_link_libraries(cod_bro_loader PRIVATE audio input online render xbox platform)
target_compile_definitions(cod_bro_loader PRIVATE
    COD_BRO_DEFAULT_SYMBOLS="${PROJECT_SOURCE_DIR}/tools/data/default_xbe_symbols.txt")


set(APP_DEPENDENT_MODULES "lib_voice"
                          "lib_unity(2.6.1)")
if (NOT BUILD_NATIVE)
    list(APPEND APP_DEPENDENT_MODULES "xscope_fileio(develop)")
endif()

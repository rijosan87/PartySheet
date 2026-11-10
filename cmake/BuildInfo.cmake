# Run with `cmake -P` from the BuildInfo custom target on EVERY build, not at configure time:
# a configure-time hash goes stale as soon as you commit without reconfiguring, which would
# stamp the wrong hash, the same mislabel this exists to catch. configure_file leaves an
# output untouched when its content is unchanged, so an unchanged tree does not recompile.
#
# Inputs (-D): SRC, OUT, GIT, PROJECT_NAME, PROJECT_VERSION, PROJECT_VERSION_MAJOR/MINOR/PATCH.
#
# "-dirty" means tracked files differed from HEAD at build time. Dev builds are usually dirty
# and get committed afterwards, so a shipped DLL typically reads "<previous commit>-dirty";
# the link timestamp in the log line tells two such builds apart.

set(PS_GIT_HASH "nogit")
if(GIT)
    execute_process(
        COMMAND "${GIT}" -C "${SRC}" rev-parse --short HEAD
        OUTPUT_VARIABLE _hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND NOT _hash STREQUAL "")
        set(PS_GIT_HASH "${_hash}")
        execute_process(
            COMMAND "${GIT}" -C "${SRC}" status --porcelain --untracked-files=no
            OUTPUT_VARIABLE _dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(NOT _dirty STREQUAL "")
            string(APPEND PS_GIT_HASH "-dirty")
        endif()
    endif()
endif()

configure_file("${SRC}/cmake/version.rc.in" "${OUT}/version.rc" @ONLY)
configure_file("${SRC}/cmake/BuildInfo.h.in" "${OUT}/include/BuildInfo.h" @ONLY)

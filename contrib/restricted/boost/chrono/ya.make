# Generated by devtools/yamaker from nixpkgs 24.05.

LIBRARY()

LICENSE(
    BSL-1.0 AND
    MIT AND
    NCSA
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.88.0)

ORIGINAL_SOURCE(https://github.com/boostorg/chrono/archive/boost-1.88.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/core
    contrib/restricted/boost/integer
    contrib/restricted/boost/move
    contrib/restricted/boost/mpl
    contrib/restricted/boost/predef
    contrib/restricted/boost/ratio
    contrib/restricted/boost/static_assert
    contrib/restricted/boost/system
    contrib/restricted/boost/throw_exception
    contrib/restricted/boost/type_traits
    contrib/restricted/boost/typeof
    contrib/restricted/boost/utility
    contrib/restricted/boost/winapi
)

ADDINCL(
    GLOBAL contrib/restricted/boost/chrono/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

IF (DYNAMIC_BOOST)
    CFLAGS(
        GLOBAL -DBOOST_CHRONO_DYN_LINK
    )
ENDIF()

SRCS(
    src/chrono.cpp
    src/process_cpu_clocks.cpp
    src/thread_clock.cpp
)

END()

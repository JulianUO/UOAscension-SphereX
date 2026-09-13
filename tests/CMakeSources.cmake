set(SPHERE_TESTS_SOURCES
    tests/src/t_num_parsing.cpp
    tests/src/t_CPointBase.cpp
    tests/src/t_CUOClientVersion.cpp
    tests/src/t_CSReferenceCount.cpp
    tests/src/t_vendor_buy.cpp
    tests/src/t_CUID.cpp
    tests/src/t_gc_placeholder.cpp
    tests/src/t_ultimalive.cpp
    tests/src/t_ultimalive_discovery.cpp
)
source_group(tests FILES ${SPHERE_TESTS_SOURCES})

#set(SPHERE_HEADERS ${SPHERE_HEADERS} ${SPHERE_TESTS_HEADERS})
set(SPHERE_SOURCES ${SPHERE_SOURCES} ${SPHERE_TESTS_SOURCES})

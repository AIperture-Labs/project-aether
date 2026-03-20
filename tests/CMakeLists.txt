include(DoctestHelpers)

add_doctest(
    NAME test_libLoadersObj
    SOURCES
    lib/Loaders/test_libLoadersObj.cpp
    LIBS
    Loaders::Obj
)
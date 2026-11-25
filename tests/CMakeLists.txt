# CMakeLists.txt - Testes do Vizu

set(TEST_SOURCES
    cdr_tests.cpp
    clean_tests.cpp
    format_tests.cpp
    office_tests.cpp
    pptx_tests.cpp
    vector_bundle_tests.cpp
    vector_tests.cpp
)

set(TEST_LIBS
    shlwapi ole32 oleaut32 rpcrt4 comctl32 gdi32 user32 shell32 advapi32 psapi
    d2d1 dwrite windowscodecs dwmapi version
)

foreach(test ${TEST_SOURCES})
    get_filename_component(test_name ${test} NAME_WE)
    add_executable(${test_name} ${test})
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

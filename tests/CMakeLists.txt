# Unit tests: one executable per test, registered with CTest under the "unit" label.
foreach(name string_table_test app_data_test)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE crisp_core)
    crisp_apply_options(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS unit)
endforeach()

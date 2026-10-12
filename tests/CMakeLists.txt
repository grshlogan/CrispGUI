# Unit tests: one executable per test, registered with CTest under the "unit" label.
set(CRISPGUI_UNIT_TESTS
    string_table_test
    app_data_test
    json_test
    jsonc_test
    sha256_test
    cli_flags_test
    profile_store_test
    launch_command_test)
foreach(name IN LISTS CRISPGUI_UNIT_TESTS)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE crisp_core)
    crisp_apply_options(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS unit)
endforeach()

# Test data next to the test executables (tests/test_support.h); tools/package-windows.sh
# copies the folder into the delivery. The reference profiles are copied byte for byte.
set(CRISPGUI_REFERENCE_PROFILES
    cohere-asr-ja-q8.conf
    kotoba-whisper-v2.2-q8.conf
    parakeet-ctc-1.1b-ja-q8.conf
    qwen3-asr-1.7b-ja-anime-q8.conf)
foreach(profile IN LISTS CRISPGUI_REFERENCE_PROFILES)
    configure_file("${PROJECT_SOURCE_DIR}/reference/crispasr/profiles-japanese/${profile}"
        "${CMAKE_CURRENT_BINARY_DIR}/testdata/profiles-japanese/${profile}" COPYONLY)
endforeach()

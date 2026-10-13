# Unit tests: one executable per test, registered with CTest under the "unit" label.
set(CRISPGUI_UNIT_TESTS
    string_table_test
    app_data_test
    json_test
    jsonc_test
    sha256_test
    cli_flags_test
    profile_store_test
    launch_command_test
    command_line_test
    server_log_test
    server_state_test
    json_writer_test
    multipart_test
    transcript_test
    srt_writer_test
    api_response_test)
foreach(name IN LISTS CRISPGUI_UNIT_TESTS)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE crisp_core)
    crisp_apply_options(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS unit)
endforeach()

# A stand-in for the CrispASR server (stub_crispasr.cpp) that the integration tests start as a
# child process; tools/package-windows.sh copies it into the delivery.
add_executable(stub_crispasr stub_crispasr.cpp)
crisp_apply_options(stub_crispasr)
if(WIN32)
    target_link_libraries(stub_crispasr PRIVATE ws2_32)
    target_link_options(stub_crispasr PRIVATE -municode)  # wmain: the arguments in UTF-16
endif()

# Integration tests: real processes, sockets, and HTTP on 127.0.0.1 with the stub, under the
# "integration" label. Each must finish within the minute run-tests.ps1 allows.
set(CRISPGUI_INTEGRATION_TESTS
    process_test
    http_test
    server_manager_test
    api_client_test
    api_probe_test)
foreach(name IN LISTS CRISPGUI_INTEGRATION_TESTS)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE crisp_core)
    if(WIN32)
        target_link_libraries(${name} PRIVATE ws2_32)
    endif()
    crisp_apply_options(${name})
    add_dependencies(${name} stub_crispasr)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS integration TIMEOUT 120)
endforeach()
add_dependencies(api_probe_test api_probe)

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
# Server answers for the parser and the stub's replay (tests/data/README.md).
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/data/verbose-synthetic.json"
    "${CMAKE_CURRENT_BINARY_DIR}/testdata/verbose-synthetic.json" COPYONLY)
# Answers recorded from the deployed server, copied byte for byte (tests/data/README.md).
set(CRISPGUI_DEPLOYED_ANSWERS
    transcript.verbose.json
    window.verbose.json
    transcript.srt
    error-key.json
    error-format.json
    error-decode.json
    health.json
    models.json)
foreach(answer IN LISTS CRISPGUI_DEPLOYED_ANSWERS)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/data/deployed-parakeet/${answer}"
        "${CMAKE_CURRENT_BINARY_DIR}/testdata/deployed-parakeet/${answer}" COPYONLY)
endforeach()

if (NOT EXISTS "${PLUGIN_BINARY}")
    message(FATAL_ERROR "Missing Windows x64 VST3 binary: ${PLUGIN_BINARY}")
endif()
if (NOT EXISTS "${STANDALONE}")
    message(FATAL_ERROR "Missing standalone executable: ${STANDALONE}")
endif()

# The plugin binary sits inside <bundle>/Contents/x86_64-win.
get_filename_component(plugin_binary_dir "${PLUGIN_BINARY}" DIRECTORY)
get_filename_component(contents_dir "${plugin_binary_dir}" DIRECTORY)
get_filename_component(bundle "${contents_dir}" DIRECTORY)
get_filename_component(bundle_name "${bundle}" NAME)
get_filename_component(bundle_parent "${bundle}" DIRECTORY)
get_filename_component(architecture "${plugin_binary_dir}" NAME)
if (NOT bundle_name MATCHES "[.]vst3$" OR NOT architecture STREQUAL "x86_64-win")
    message(FATAL_ERROR "Unexpected Windows x64 VST3 layout: ${PLUGIN_BINARY}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf
        "${OUTPUT_DIR}/ANKS-SWARM-01-${VERSION}-Windows-x64-VST3.zip"
        --format=zip "${bundle_name}"
    WORKING_DIRECTORY "${bundle_parent}"
    COMMAND_ERROR_IS_FATAL ANY)

get_filename_component(standalone_name "${STANDALONE}" NAME)
get_filename_component(standalone_dir "${STANDALONE}" DIRECTORY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf
        "${OUTPUT_DIR}/ANKS-SWARM-01-${VERSION}-Windows-x64-Portable.zip"
        --format=zip "${standalone_name}"
    WORKING_DIRECTORY "${standalone_dir}"
    COMMAND_ERROR_IS_FATAL ANY)

cmake_minimum_required(VERSION 3.25)
set(CURRENT_VERSION "0.9.0")

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PamplejuceVersion)

# Project setup
project(CTD201 VERSION ${CURRENT_VERSION})

# JUCE submodule
add_subdirectory(JUCE)

# CLAP extension
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Melatonin inspector module
add_subdirectory(modules/melatonin_inspector)

# JUCE plugin target
juce_add_plugin(CTD201
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "OrsonDev1"
        BUNDLE_ID "com.CTD201.CTD201"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE ORSO
        PLUGIN_CODE C201
        FORMATS Standalone AU VST3 AUv3
        PRODUCT_NAME "Cosmic Tape Delay 201"
)

# --- Binary Data ---
juce_add_binary_data(CTD201BinaryData
        SOURCES
        Resources/DefaultReverbIR.wav
)

# Link BinaryData to plugin target
target_link_libraries(CTD201 PRIVATE CTD201BinaryData)

# SharedCode library for your .h/.cpp
add_library(SharedCode INTERFACE)
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

# Link modules and libraries
target_link_libraries(SharedCode
        INTERFACE
        melatonin_inspector
        juce_audio_utils
        juce_audio_processors
        juce_dsp
        juce_gui_basics
        juce_gui_extra
        CTD201BinaryData
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_link_libraries(CTD201 PRIVATE SharedCode)

# Optional includes
include(PamplejuceMacOS)
include(JUCEDefaults)
include(SharedCodeDefaults)
include(PamplejuceIPP)
include(Tests)
include(Benchmarks)
include(GitHubENV)
include(XcodePrettify)

# Preprocessor definitions
target_compile_definitions(SharedCode INTERFACE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        VERSION="${CURRENT_VERSION}"
        PRODUCT_NAME_WITHOUT_VERSION="CTD201"
)
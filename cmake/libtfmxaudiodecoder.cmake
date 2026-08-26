set(TFMX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtfmxaudiodecoder")
set(TFMX_SRC "${TFMX_ROOT}/src")

set(TFMX_SOURCES
  ${TFMX_SRC}/tfmxaudiodecoder.cpp
  ${TFMX_SRC}/Decoder.cpp
  ${TFMX_SRC}/DecoderProxy.cpp
  ${TFMX_SRC}/CRCLight.cpp
  ${TFMX_SRC}/Dump.cpp
  ${TFMX_SRC}/Filter.cpp
  ${TFMX_SRC}/PaulaVoice.cpp
  ${TFMX_SRC}/LamePaulaVoice.cpp
  ${TFMX_SRC}/LamePaulaMixer.cpp
  ${TFMX_SRC}/Chris/TFMXDecoder.cpp
  ${TFMX_SRC}/Chris/Macro.cpp
  ${TFMX_SRC}/Chris/Modulation.cpp
  ${TFMX_SRC}/Chris/Pattern.cpp
  ${TFMX_SRC}/Chris/Sequencer.cpp
  ${TFMX_SRC}/Chris/Songs.cpp
  ${TFMX_SRC}/Chris/ByChecksum.cpp
  ${TFMX_SRC}/Chris/SamplesFile.cpp
  ${TFMX_SRC}/Chris/MergedFiles.cpp
  ${TFMX_SRC}/Chris/DNS/DNSDecoder.cpp
  ${TFMX_SRC}/Jochen/HippelDecoder.cpp
  ${TFMX_SRC}/Jochen/Analyze.cpp
  ${TFMX_SRC}/Jochen/COSO.cpp
  ${TFMX_SRC}/Jochen/Envelope.cpp
  ${TFMX_SRC}/Jochen/FC.cpp
  ${TFMX_SRC}/Jochen/Instrument.cpp
  ${TFMX_SRC}/Jochen/MCMD.cpp
  ${TFMX_SRC}/Jochen/ModPack.cpp
  ${TFMX_SRC}/Jochen/Portamento.cpp
  ${TFMX_SRC}/Jochen/Probe.cpp
  ${TFMX_SRC}/Jochen/SMOD.cpp
  ${TFMX_SRC}/Jochen/TFMX7V.cpp
  ${TFMX_SRC}/Jochen/TFMX.cpp
  ${TFMX_SRC}/Jochen/TraitsByChecksum.cpp
  ${TFMX_SRC}/Jochen/Vibrato.cpp
)

add_library(tfmxaudiodecoder STATIC ${TFMX_SOURCES})
set_property(TARGET tfmxaudiodecoder PROPERTY FOLDER "thirdparty")

target_include_directories(tfmxaudiodecoder PUBLIC
  ${TFMX_SRC}
  ${TFMX_SRC}/Chris
  ${TFMX_SRC}/Chris/DNS
  ${TFMX_SRC}/Jochen
)

target_compile_features(tfmxaudiodecoder PUBLIC cxx_std_11)

if(MSVC)
  target_compile_definitions(tfmxaudiodecoder PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(tfmxaudiodecoder PRIVATE -Wno-unused-parameter -Wno-sign-compare)
endif()

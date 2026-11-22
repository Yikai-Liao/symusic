function(replace_sndfile_text path old new)
    set(file "${sndfile_source}/${path}")
    file(READ "${file}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(FIND "${content}" "${old}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Expected libsndfile 1.2.2 text in ${path}: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" content "${content}")
    file(WRITE "${file}" "${content}")
endfunction()

replace_sndfile_text(cmake/SndFileChecks.cmake
    "Vorbis_FOUND AND FLAC_FOUND AND Opus_FOUND" "Vorbis_FOUND")
foreach(line IN ITEMS
        "find_package (FLAC CONFIG)" "find_package (Opus CONFIG)"
        "find_package (FLAC)" "find_package (Opus)"
        "find_package_handle_standard_args (FLAC CONFIG_MODE)"
        "find_package_handle_standard_args (Opus CONFIG_MODE)")
    replace_sndfile_text(cmake/SndFileChecks.cmake "${line}" "")
endforeach()
foreach(codec IN ITEMS FLAC::FLAC Opus::opus)
    replace_sndfile_text(CMakeLists.txt
        "$<$<BOOL:\${HAVE_EXTERNAL_XIPH_LIBS}>:${codec}>" "")
endforeach()
foreach(codec IN ITEMS flac ogg_opus)
    replace_sndfile_text(src/${codec}.c "#if HAVE_EXTERNAL_XIPH_LIBS" "#if 0")
endforeach()
# Keep the reported format tables consistent with the disabled codecs.
foreach(format IN ITEMS "SF_FORMAT_FLAC | SF_FORMAT_PCM_16" "SF_FORMAT_FLAC," "SF_FORMAT_OGG | SF_FORMAT_OPUS")
    replace_sndfile_text(src/command.c
        "#if HAVE_EXTERNAL_XIPH_LIBS\n\t{\t${format}" "#if 0\n\t{\t${format}")
endforeach()
replace_sndfile_text(src/command.c "\t{\tSF_FORMAT_OPUS,\t\t\t\"Opus\",\t\t\t\t\tNULL \t},\n" "")

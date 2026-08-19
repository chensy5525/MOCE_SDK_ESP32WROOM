idf_component_register(
    SRCS "main.c" "audio_core.c" "audio_mp3.c"
    INCLUDE_DIRS "."
    REQUIRES esp_audio_codec fatfs max98357a_direct wear_levelling
)

fatfs_create_spiflash_image(
    audio
    "${CMAKE_CURRENT_LIST_DIR}/../audio_data"
    FLASH_IN_PROJECT
)

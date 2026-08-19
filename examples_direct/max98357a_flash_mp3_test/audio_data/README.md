# Flash audio asset

Place one MP3 file here as `test.mp3` before building. MP3 assets are ignored by
Git so copyrighted audio is not committed accidentally.

The example's 4 MiB partition table reserves `0x270000` bytes for the FAT image.
Keep the complete `audio_data` directory below that size, including filesystem
overhead. The build command creates and flashes this FAT image together with the
application when `idf.py flash` is used.

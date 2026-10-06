# Import fixtures (ADR-0054)

Small files for the `[import-formats]` gates in `tests/audio_import_tests.cpp`. The lossless formats and Ogg
Vorbis are written by the tests themselves with JUCE's writers; MP3 has no encoder in the build, so these
files are committed. They were made with ffmpeg 7.1.1 (gyan.dev full build, libmp3lame) on 2026-10-06:

```
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.5" -f lavfi -i "sine=frequency=660:sample_rate=48000:duration=0.5" -filter_complex "[0:a][1:a]amerge=inputs=2,volume=0.5" -c:a pcm_s16le -bitexact source_48k_stereo.wav
ffmpeg -i source_48k_stereo.wav -c:a libmp3lame -b:a 128k -bitexact cbr128_info.mp3
ffmpeg -i source_48k_stereo.wav -c:a libmp3lame -b:a 128k -write_xing 0 -bitexact cbr128_noinfo.mp3
ffmpeg -i source_48k_stereo.wav -c:a libmp3lame -q:a 4 -bitexact vbr_xing.mp3
ffmpeg -i source_48k_stereo.wav -c:a libmp3lame -b:a 128k -write_xing 0 -id3v2_version 0 -write_id3v1 1 -metadata title=yesdaw -bitexact cbr128_id3v1.mp3
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.5" -c:a libmp3lame -b:a 64k -bitexact mono_44k.mp3
```

- `cbr128_info.mp3` — constant bit rate with a LAME "Info" header (the length is read from it).
- `cbr128_noinfo.mp3` — the same without the header (JUCE estimates the length from the stream size).
- `vbr_xing.mp3` — variable bit rate with a Xing header.
- `cbr128_id3v1.mp3` — no length header, a 128-byte ID3v1 tag at the end.
- `mono_44k.mp3` — mono at 44.1 kHz: a cross-rate file (refused until the cross-rate checkpoint).

The tests pin each MP3's decoded length (JUCE 8.0.4's reader) and check it on every CI platform.

# This file is generated by gyp; do not edit.

export builddir_name ?= trunk/webrtc/common_audio/out
.PHONY: all
all:
	$(MAKE) -C ../.. signal_processing_neon signal_processing resampler vad vad_unittests signal_processing_unittests resampler_unittests
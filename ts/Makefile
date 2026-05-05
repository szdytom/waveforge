# WaveForge JS Scene — TypeScript build
# Requires: esbuild (npm install -g esbuild)
#
# Compile TS sources from ts/ to assets/bundled-js/.
# The output is plain JS that QuickJS can evaluate directly.

ESBUILD ?= esbuild
SRC_DIR  := .
OUT_DIR  := ../assets/bundled-js
ES_TARGET := es2020

TS_SRCS := $(wildcard $(SRC_DIR)/scene-*.ts)
JS_OUTS := $(patsubst $(SRC_DIR)/%.ts, $(OUT_DIR)/%.js, $(TS_SRCS))

.PHONY: all clean

all: $(JS_OUTS)

$(OUT_DIR)/%.js: $(SRC_DIR)/%.ts $(SRC_DIR)/waveforge.d.ts
	$(ESBUILD) $< \
		--target=$(ES_TARGET) \
		--format=esm \
		--tree-shaking=true \
		--platform=neutral \
		--minify \
		--outfile=$@

clean:
	rm -f $(JS_OUTS)

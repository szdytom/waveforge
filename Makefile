BUNDLED_JS_DIR := assets/bundled-js
TS_SRC_DIR := ts-src
TS_FILES := $(filter-out $(TS_SRC_DIR)/%.d.ts, $(wildcard $(TS_SRC_DIR)/*.ts))
JS_FILES := $(patsubst $(TS_SRC_DIR)/%.ts, $(BUNDLED_JS_DIR)/%.js, $(TS_FILES))

.PHONY: all clean

all: $(JS_FILES)

$(BUNDLED_JS_DIR)/%.js: $(TS_SRC_DIR)/%.ts
	@mkdir -p $(BUNDLED_JS_DIR)
	esbuild $< --outfile=$@ --target=es2020

clean:
	rm -rf $(BUNDLED_JS_DIR)

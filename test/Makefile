BUILD ?= build

SOURCES=$(wildcard *.c)
BITCODES=$(patsubst %.c,$(BUILD)/%-tgt.bc,$(SOURCES))

.PHONY: all
all: $(BITCODES)

.PHONY: clean
clean:
	rm -r $(BUILD)

# Create the output build directory.
$(BUILD):
	mkdir -p $@

# Compile the test to bitcode.
.INTERMEDIATE: $(BUILD)/%.bc
$(BUILD)/%.bc: %.c $(BUILD)
	clang -o $@ -c $< -emit-llvm -O0 -Xclang -disable-O0-optnone

# Normalize the bitcode.
.PRECIOUS: $(BUILD)/%-src.bc
$(BUILD)/%-src.bc: $(BUILD)/%.bc
	opt $< -o $@ -passes="mem2reg,sroa,instcombine,simplifycfg,lcssa"

# Optimize the bitcode.
.PRECIOUS: $(BUILD)/%-tgt.bc
$(BUILD)/%-tgt.bc: $(BUILD)/%-src.bc
	opt $< -o $@ -passes="$*"
	llvm-diff $< $@ && rm $< $@ || true

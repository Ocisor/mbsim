# mbsim: builds the runtime library that every user program links against.
#
#   make            native library   -> ~/.cache/mbsim/lib-native.a
#   make asan       AddressSanitizer -> ~/.cache/mbsim/lib-native-asan.a
#   make arm        32-bit ARM       -> ~/.cache/mbsim/lib-arm.a   (needs the cross compiler)
#   make clean      remove everything under ~/.cache/mbsim
#
# Nothing is ever written into the source tree or into a user's project folder.

CACHE    ?= $(HOME)/.cache/mbsim
CXX      ?= g++
ARMCXX   ?= arm-linux-gnueabihf-g++
ARMAR    ?= arm-linux-gnueabihf-ar

SRCS     := $(wildcard runtime/*.cpp) $(wildcard ui/*.cpp)
HDRS     := $(wildcard include/*.h include/mbsim/*.h runtime/*.h runtime/*.inc)
CXXFLAGS := -std=gnu++17 -O2 -g -Wall -Wextra -Iinclude -Iruntime -pthread
# Thumb-2, with integer divide as on the Cortex-M4. "+fp" keeps the hard-float ABI usable.
ARMFLAGS := -march=armv7ve+fp -mthumb

objs = $(patsubst %.cpp,$(CACHE)/obj-$(1)/%.o,$(SRCS))

.PHONY: all native asan arm clean
all: native
native: $(CACHE)/lib-native.a
asan:   $(CACHE)/lib-native-asan.a
arm:    $(CACHE)/lib-arm.a

$(CACHE)/lib-native.a: $(call objs,native)
	@rm -f $@ && ar rcs $@ $^ && echo "built $@"

$(CACHE)/lib-native-asan.a: $(call objs,native-asan)
	@rm -f $@ && ar rcs $@ $^ && echo "built $@"

$(CACHE)/lib-arm.a: $(call objs,arm)
	@rm -f $@ && $(ARMAR) rcs $@ $^ && echo "built $@"

$(CACHE)/obj-native/%.o: %.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CACHE)/obj-native-asan/%.o: %.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DMBSIM_ASAN -fsanitize=address,undefined -fno-omit-frame-pointer -c $< -o $@

$(CACHE)/obj-arm/%.o: %.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(ARMCXX) $(CXXFLAGS) $(ARMFLAGS) -c $< -o $@

clean:
	rm -rf $(CACHE)

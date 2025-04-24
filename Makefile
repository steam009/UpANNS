# Setting defaults

PROJECT             = host
EXE_DPU             ?= search_dpu
BUILD_DIR           = build

NR_DPUS             ?= 512
NR_TASKLETS         ?= 16
COUNTER_CONFIG      ?= "COUNT_CYCLES"
SHOW_DPU_LOGS       ?= 1
# NR_TABLES           ?= 2

MS                   ?= 32
KSUB                ?= 256
DSUB                ?= 4
MAX_DPU_STORE_SIZE  ?= 1024
CODE_SIZE           ?= 32
TOPK                ?= 100
MAX_Q_O             ?= 8
MAX_PROBE_NUM       ?= 16
DIMM                ?= 128
MAX_DPU_ID          ?= 8
CODEBOOK_SIZE       ?= 4096
ETD                 ?= 16
NPROBS              ?= 64







RT_CONFIG           ?= "ALL"

# TEST with c_test.py
DPU_TEST            ?= 0

ifeq ($(DPU_TEST),1)
  NR_COLS             ?= ${NR_COLS}
  NR_DPUS             = 256
else
  NR_COLS             ?= 8   # Change this
  NR_DPUS             ?= 256
endif

# Version information
VERSION             = 0.0.0

# Application sources and artifacts
APP_BIN             = $(BUILD_DIR)/$(PROJECT)
APP_SOURCES         =
# APP_MAIN            = src/emb_host.c
APP_MAIN            = search_host.cpp
APP_OBJS            = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(APP_SOURCES) $(APP_MAIN))

# Includes
INC                 = -Iinclude -Ifaiss -L/home/cst

# Test sources and artifacts
TEST_BIN            = $(BUILD_DIR)/$(PROJECT)_tests
TEST_SOURCES        = $(APP_SOURCES) tests/main.c
TEST_OBJS           = $(patsubst %.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SOURCES))

# Generated dependency files
DEPS                = $(APP_OBJS:.o=.d) \
                      $(TEST_OBJS:.o=.d)

# Compiler options
CC                  = g++
COMMON_CFLAGS       = -lm --std=c++11 -fPIC -lstdc++ # -Wall -Wextra -MMD -Werror
SHARED_CFLAGS       = -shared -Wl,-soname,$(PROJECT)
DPU_OPTS            = `dpu-pkg-config --cflags --libs dpu` -L/home/cst -lfaiss -fopenmp

# Tools
CLANG_FORMAT        = clang-format

# Debug/Release mode
ifneq ($(DEBUG),)
  COMMON_CFLAGS     += -g -DDEBUG
  BUILD_DIR         := $(BUILD_DIR)/debug
else
  COMMON_CFLAGS     += -O3
  BUILD_DIR         := $(BUILD_DIR)/release
endif

CFLAGS              += $(COMMON_CFLAGS) \
                       -DVERSION=$(VERSION) \
                       -DCOUNTER_CONFIG=$(COUNTER_CONFIG) \
					             -DMS=$(MS) \
                       -DKSUB=$(KSUB) \
                       -DDSUB=$(DSUB) \
                       -DMAX_DPU_STORE_SIZE=$(MAX_DPU_STORE_SIZE) \
                       -DCODE_SIZE=$(CODE_SIZE) \
                       -DTOPK=$(TOPK) \
                       -DMAX_Q_O=$(MAX_Q_O) \
                       -DMAX_PROBE_NUM=$(MAX_PROBE_NUM) \
					             -DDIMM=$(DIMM) \
                       -DMAX_DPU_ID=$(MAX_DPU_ID) \
                       -DCODEBOOK_SIZE=$(CODEBOOK_SIZE) \
					             -DNR_DPUS=$(NR_DPUS) \
                       -DNR_TASKLETS=$(NR_TASKLETS) \
                       -DETD=$(ETD) \
                       -DNPROBS=$(NPROBS) \
					   

# define SHOW_DPU_LOGS in the source if we want DPU logs
ifeq ($(SHOW_DPU_LOGS), 1)
	CFLAGS+=-DSHOW_DPU_LOGS
endif

# Silence make
ifneq ($(V),)
  SILENCE           =
else
  SILENCE           = @
endif

# Fancy output
SHOW_COMMAND        := @printf "%-15s%s\n"
SHOW_CC             := $(SHOW_COMMAND) "[ $(CC) ]"
SHOW_CLEAN          := $(SHOW_COMMAND) "[ CLEAN ]"
SHOW_GEN            := $(SHOW_COMMAND) "[ GEN ]"
SHOW_MAKE           := $(SHOW_COMMAND) "[ MAKE ]"
SHOW_FORMAT         := $(SHOW_COMMAND) "[ FORMAT ]"

##############################################################################################
# Default target and help message
##############################################################################################
DEFAULT_TARGET =  $(APP_BIN)

all: $(DEFAULT_TARGET) dpu
.PHONY: all

# Take care of compiler generated depedencies
-include $(DEPS)

##############################################################################################
# Application
##############################################################################################
$(APP_BIN): $(APP_OBJS)
	$(SHOW_CC) $@
	$(SILENCE)$(CC) -o $@ $(APP_OBJS) $(DPU_OPTS)

# $(APP_LIB): $(APP_OBJS)
# 	$(SHOW_CC) $@
# 	$(SILENCE)$(CC) -o $@ $(APP_OBJS) $(DPU_OPTS) $(SHARED_CFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	$(SHOW_CC) $@
	$(SILENCE)mkdir -p $(dir $@)
	$(SILENCE)$(CC) $(CFLAGS) $(INC) -c $< -o $@ $(DPU_OPTS)

##############################################################################################
# DPU Application
##############################################################################################

export BUILD_DIR
export DEBUG
export NR_DPUS
export NR_TASKLETS
export COUNTER_CONFIG
export EXE_DPU
export MS      
export KSUB   
export DSUB          
export MAX_DPU_STORE_SIZE 
export CODE_SIZE     
export TOPK         
export MAX_Q_O   
export MAX_PROBE_NUM 
export DIMM  
export MAX_DPU_ID 
export CODEBOOK_SIZE
export ETD
export NPROBS

dpu:
	$(SHOW_MAKE) $@
	$(SILENCE)$(MAKE) -C dpu

##############################################################################################
# Tests
##############################################################################################



##############################################################################################
# Cleanup
##############################################################################################
clean:
	$(SHOW_CLEAN) $(BUILD_DIR)
	$(SILENCE)rm -rf $(BUILD_DIR)

.PHONY: clean

##############################################################################################
# Format
##############################################################################################
format:
	$(SHOW_FORMAT) $@
	$(SILENCE)$(CLANG_FORMAT) -i *.cpp include/*.h 
	$(SILENCE)$(MAKE) format -C dpu

.PHONY: format

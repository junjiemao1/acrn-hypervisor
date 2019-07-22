# usage: override_config <symbol> <default>
#
# Given a configuration symbol (without the CONFIG_ prefix), this macro
# overrides its value as follows.
#    1. If a value is specified from command line, that value is used.
#    2. If neither config.mk nor the command line specifies a value, the given
#       default is used.
define override_config =
ifdef $(1)
CONFIG_$(1) := $($(1))
else ifndef CONFIG_$(1)
CONFIG_$(1) := $(2)
endif
endef

# Backward-compatibility for RELEASE=(0|1)
ifdef RELEASE
ifeq ($(RELEASE),1)
override RELEASE := y
else
override RELEASE := n
endif
endif

ifeq ($(UT),1)
SCENARIO_NAME := unit_test
else
SCENARIO_NAME := logical_partition
endif

include include/config.mk
$(eval $(call override_config,RELEASE,n))

CFLAGS += -include include/config.h

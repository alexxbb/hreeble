OS_NAME := $(shell uname -s)
SOURCES = hreeble/Element.cpp hreeble/sop_hreeble.cpp

ifeq ($(OS_NAME),Darwin)
	INSTDIR = /Users/$(USER)/Library/Preferences/houdini/15.0
	DSONAME = sop_hreeble.dylib
	OPTIMIZER = -O1
else
	INSTDIR = $(HOME)/houdini15.0
	DSONAME = sop_hreeble.so
	OPTIMIZER = -O2
endif
CXXFLAGS+=-std=c++11
# OPTIMIZER = -g

include $(HFS)/toolkit/makefiles/Makefile.gnu

# all:	install clean
# install:	default	clean
# 	@if [ ! -d $(INSTDIR)/dso ]; then mkdir $(INSTDIR)/dso; fi
# 	@mv $(DSONAME) $(INSTDIR)/dso
# clean:
# 	@rm hreeble/*.o

#
# $Id$
#

TOP = .
include $(TOP)/configure/CONFIG

DIRS += config configure src

include $(TOP)/configure/RULES_TOP

release::
	@echo TOP: Creating Release...
	@$(TOOLS)/MakeRelease

built_release::
	@echo TOP: Creating Fully Built Release...
	@$(TOOLS)/MakeRelease -b $(INSTALL_LOCATION)
	

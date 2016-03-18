SUBDIRS = src/bwt

LIBBWT_CODE = src/bwt

.PHONY: all clean install libbwt

all: $(SUBDIRS)
	$(MAKE) -C $(SUBDIRS)

clean: $(SUBDIRS)
	$(MAKE) -C $(SUBDIRS) clean

install: $(SUBDIRS)
	$(MAKE) -C $(SUBDIRS) install

libbwt:
	$(MAKE) -C $(LIBBWT_CODE)

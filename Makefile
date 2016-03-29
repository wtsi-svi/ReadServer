LIBBWT_CODE = src/bwt

SERVICE_CODE = src/service

UTIL_CODE = src/util

.PHONY: all clean install libbwt service util

all:
	$(MAKE) -C $(LIBBWT_CODE)
	$(MAKE) -C $(LIBBWT_CODE) install
	$(MAKE) -C $(SERVICE_CODE)
	$(MAKE) -C $(UTIL_CODE)

clean:
	$(MAKE) -C $(LIBBWT_CODE) clean
	$(MAKE) -C $(SERVICE_CODE) clean
	$(MAKE) -C $(UTIL_CODE) clean

install:
	$(MAKE) -C $(LIBBWT_CODE) install
	$(MAKE) -C $(SERVICE_CODE) install
	$(MAKE) -C $(UTIL_CODE) install

libbwt:
	$(MAKE) -C $(LIBBWT_CODE)

service:
	$(MAKE) -C $(LIBBWT_CODE)
	$(MAKE) -C $(LIBBWT_CODE) install
	$(MAKE) -C $(SERVICE_CODE)

util:
	$(MAKE) -C $(UTIL_CODE)

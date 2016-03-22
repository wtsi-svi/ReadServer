LIBBWT_CODE = src/bwt

SERVICE_CODE = src/service

.PHONY: all clean install libbwt service

all:
	$(MAKE) -C $(LIBBWT_CODE)
	$(MAKE) -C $(LIBBWT_CODE) install
	$(MAKE) -C $(SERVICE_CODE)

clean:
	$(MAKE) -C $(LIBBWT_CODE) clean
	$(MAKE) -C $(SERVICE_CODE) clean

install:
	$(MAKE) -C $(LIBBWT_CODE) install
	$(MAKE) -C $(SERVICE_CODE) install

libbwt:
	$(MAKE) -C $(LIBBWT_CODE)

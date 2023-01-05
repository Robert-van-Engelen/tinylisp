MAKE := make
all:
	$(MAKE) -C ./test-framework
	$(MAKE) -C ./src
clean:
	$(MAKE) -C ./test-framework clean
	$(MAKE) -C ./src clean

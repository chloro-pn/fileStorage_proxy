.PHONY : all
all:
	@echo "making project.";\
	$(MAKE) -C ./examples;

.PHONY : clean
clean:
	@echo "cleaning project.";\
	$(MAKE) -C ./src clean;\
	$(MAKE) -C ./examples clean;

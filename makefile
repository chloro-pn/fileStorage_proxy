.PHONY all clean

all:
	@echo "making project."; \
	$(MAKE) -C src;

clean:
	@echo "cleaning project."; \
	$(MAKE) -C src clean;

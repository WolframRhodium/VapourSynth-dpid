CXX ?= clang++
CFLAGS = -O2 --std=c++11 -Wall -Werror -Wextra -Wno-unused-parameter -fPIC
CUFLAGS = -Xcompiler -Wall -Xcompiler -Werror -Xcompiler -Wextra -Xcompiler -Wno-unused-parameter -Xcompiler -fPIC
LIBDIRS = /opt/cuda/lib64
LIBS = cudart
INCLUDE = /opt/cuda/include /usr/include
TARGET ?= libdpid.so
PREFIX ?= /usr/local/lib/vapoursynth

$(TARGET): dpid.o kernel8.o kernel16.o
	@echo "Linking target..."
	@nvcc -shared -m64 $(CUFLAGS) $(addprefix -l,$(LIBS)) $(addprefix -L,$(LIBDIRS)) dpid.o kernel8.o kernel16.o -o $(TARGET)

dpid.o: dpid.cpp dpid.h
	@echo "Compiling dpid.cpp ..."
	@$(CXX) $(CFLAGS) -m64 -c $(addprefix -I,$(INCLUDE)) $< -o $@

kernel8.o: kernel8.cu
	@echo "Compiling kernel8.cu ..."
	@nvcc $(addprefix -I,$(INCLUDE)) -m64 -c $(CUFLAGS) $< -o $@

kernel16.o: kernel16.cu
	@echo "Compiling kernel16.cu ..."
	@nvcc $(addprefix -I,$(INCLUDE)) -m64 -c $(CUFLAGS) $< -o $@

install: $(TARGET)
	@echo "Installing..."
	@cp $(TARGET) $(PREFIX)
	@chmod 644 $(PREFIX)/$(TARGET)

uninstall:
	@echo "Uninstalling..."
	@rm $(PREFIX)/$(TARGET)

.phony clean:
	@echo "Cleaning..."
	@rm -rf *.o

distclean: clean
	@rm -rf $(TARGET)

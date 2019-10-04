CXX ?= clang++
CFLAGS = -O2 --std=c++17 -Wall -Werror -Wextra -Wno-unused-parameter -fPIC
TARGET ?= libdpid.so
PREFIX ?= /usr/local/lib/vapoursynth

$(TARGET): dpid.cpp
	@echo "Compiling dpid.cpp ..."
	@$(CXX) $(CFLAGS) -shared -m64 -c $(addprefix -I,$(INCLUDE)) $< -o libdpid.so

install: $(TARGET)
	@echo "Installing..."
	@install -Dm644 $(TARGET) $(PREFIX)/$(TARGET)

uninstall:
	@echo "Uninstalling..."
	@rm $(PREFIX)/$(TARGET)

.phony clean:
	@echo "Cleaning..."
	@rm -rf *.o

distclean: clean
	@rm -rf $(TARGET)

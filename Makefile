CXXFLAGS=-Wall -Wextra -W -Wno-unused-parameter
GENERATED_HEADERS=headers/module.h headers/design.h
GENERATED_SOURCE=src/vpi_user.cpp

all: generate-api run-test

generate-api: $(GENERATED_SOURCE)

$(GENERATED_SOURCE): model_gen.tcl model/uhdm.yaml templates/vpi_user.cpp
	tclsh ./model_gen.tcl model/models.lst

unittest: src/vpi_user.cpp
	$(CXX) $(CXXFLAGS) -std=c++14 -g -Iinclude src/main.cpp src/vpi_user.cpp src/Serializer.cpp src/UHDM.capnp.c++ -I. -lcapnp -lkj -o $@

run-test: unittest
	./unittest

clean:
	rm -f $(GENERATED_SOURCE) $(GENERATED_HEADERS) unittest

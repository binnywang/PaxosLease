APP_NAME=node_server

APP_SRC=$(wildcard *.cpp)
APP_OBJ=$(APP_SRC:.cpp=.o)
APP_EXE=$(APP_NAME)

PB_DEF=message.proto

PB_SRC=$(basename $(PB_DEF)).pb.cc
PB_OBJ=$(PB_SRC:.cc=.o)

PB_LIB=-lprotobuf


CXX=g++
CXXFLAGS= -g -Wall -std=c++11
LIBS= -lpthread $(PB_LIB)


.PHONY: all print clean

all: $(APP_EXE)


$(APP_OBJ): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(PB_SRC):
	protoc  --cpp_out=. $(PB_DEF)
	

$(PB_OBJ): %.o: %.cc 
	$(CXX) -c  $(CXXFLAGS) $< -o $@


$(APP_EXE): $(PB_OBJ) $(APP_OBJ)
	$(CXX) $^ -o $(APP_EXE) $(CXXFLAGS) $(LIBS)

clean:
	rm -f *.o
	rm -f $(APP_EXE)

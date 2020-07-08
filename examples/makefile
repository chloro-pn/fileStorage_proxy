CXX := g++
INCLUDE_PATH := -I ../include -I ../third_party
CXXFLAGS := -Wall -std=c++11 -g
LIBS := -L../src -lfilest -lpthread

#### spdlog ###
INCLUDE_PATH += -I ../third_party/spdlog-1.6.1/include

#### asio ###
INCLUDE_PATH += -I ../third_party/asio/include

#### redis ###
INCLUDE_PATH += -I /file_storage/hiredis/hiredis-0.14.1
LIBS += -L/file_storage/hiredis/hiredis-0.14.1/ -lhiredis

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.out)
DEPS := $(SRCS:.cpp=.d)

.PHONY : all
all : $(OBJS)

%.out : %.cpp src/libfilest.a
	$(CXX) -o $@ $< -MMD -MP $(INCLUDE_PATH) $(LIBS) $(CXXFLAGS)

src/libfilest.a :
	@echo "try to make in ../src";\
	$(MAKE) -C ../src;

-include $(DEPS)

.PHONY : clean
clean :
	@rm -rf $(OBJS) $(DEPS) *.txt file_storage_dir/* core.*


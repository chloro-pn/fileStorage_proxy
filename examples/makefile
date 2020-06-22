CXX := g++
INCLUDE_PATH := -I ../include -I ../third_party
CXXFLAGS := -Wall -std=c++11
LIBS := -L../src -lfilest -lpthread

#### spdlog ###
INCLUDE_PATH += -I /home/pn/shared/spdlog-1.6.1/include

#### asio ###
INCLUDE_PATH += -I /home/pn/shared/asio/include

#### redis ###
INCLUDE_PATH += -I /home/pn/shared/hiredis-0.14.1
LIBS += -L/home/pn/shared/hiredis-0.14.1/ -lhiredis

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.out)
DEPS := $(SRCS:.cpp=.d)

.PHONY : all
all : $(OBJS)

$(OBJS):%.out : %.cpp src/libfilest.a
	$(CXX) -o $@ $< -MMD -MP $(INCLUDE_PATH) $(LIBS) $(CXXFLAGS)

src/libfilest.a :
	@echo "try to make in ./src";\
	$(MAKE) -C ../src;

-include $(DEPS)

.PHONY : clean
clean :
	@rm -rf $(OBJS) $(DEPS) *.txt;


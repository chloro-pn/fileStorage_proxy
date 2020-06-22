CXX := g++
INCLUDE_PATH := -I ../include -I ../third_party
CXXFLAGS := -Wall -std=c++11
LIBS := -lpthread 

#### spdlog ###
INCLUDE_PATH += -I /home/pn/shared/spdlog-1.6.1/include

#### redis ###
INCLUDE_PATH += -I /home/pn/shared/hiredis-0.14.1
LIBS += -L/home/pn/shared/hirediis-0.14.1/ -lhiredis

#### asio ###
INCLUDE_PATH += -I /home/pn/shared/asio/include

SRCS := $(wildcard client/*.cpp) $(wildcard common/*.cpp) $(wildcard common/asio_wrapper/*.cpp) $(wildcard proxy/*.cpp sserver/*.cpp)

OBJS := $(SRCS:.cpp=.o)
DEPS := $(SRCS:.cpp=.d)

TARGET := libfilest.a
	
$(TARGET) : $(OBJS)
	ar crv $@ $^

%.o : %.cpp
	$(CXX) -o $@ -c $< -MMD -MP $(INCLUDE_PATH) $(CXXFLAGS)

-include $(DEPS)

.PHONY : clean
clean :
	@rm -rf $(OBJS) $(DEPS) $(TARGET);


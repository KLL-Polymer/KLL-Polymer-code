CXX := g++
CXXFLAGS := -std=c++17 -O2 -g -MMD -MP
INCLUDES := -I. -IBaseline 

TARGET := test
SRCS := main.cc
OBJS := $(SRCS:.cc=.o)
DEPS := $(SRCS:.cc=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

-include $(DEPS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
    
clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

.PHONY: all clean

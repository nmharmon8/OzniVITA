CXX = g++
CXXFLAGS = -Wall -std=c++11
INCLUDES = -I/usr/local/include/VITA49 
LDFLAGS = -L/usr/local/lib
LIBS = -lVITA49 -luuid

TARGET = rh_file_parser
SRCS = rh_file_parser.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
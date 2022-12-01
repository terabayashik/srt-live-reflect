CXX      = g++
CFLAGS   = -Wall -g -O2 -std=c++14
TARGET   = srt-live-reflect
INCDIR   = -I./src -I/usr/local/include/srt
LIBDIR   = 
LIBS     = -lsrt -lpthread -lboost_thread -lboost_json -lcurl
SRCDIR   = ./src
OBJDIR   = ./obj
BINDIR   = ./bin
SRCS     = $(wildcard $(SRCDIR)/*.cpp) 
OBJS     = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))


all: prepare $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LIBDIR) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) $(INCDIR) -c $< -o $@

prepare:
	mkdir -p $(BINDIR)
	mkdir -p $(OBJDIR)

clean:
	rm -f $(OBJDIR)/*.o
	rm -rf $(BINDIR)/*


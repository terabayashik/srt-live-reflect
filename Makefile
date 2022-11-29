CXX      = g++
CFLAGS   = -Wall -g -std=c++14
TARGET   = srt-live-reflect
INCDIR   = -I./src -I/usr/local/include/srt
LIBDIR   = 
LIBS     = -lsrt -lpthread -lboost_thread
SRCDIR   = ./src
OBJDIR   = ./obj
BINDIR   = ./bin
SRCS     = $(wildcard $(SRCDIR)/*.cpp) 
OBJS     = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))


all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(BINDIR)
	$(CXX) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LIBDIR) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p `dirname $@`
	$(CXX) $(CFLAGS) $(INCDIR) -c $< -o $@ $(LIBDIR) $(LIBS)

clean:
	rm -f $(OBJDIR)/*.o
	rm -rf $(BINDIR)/*


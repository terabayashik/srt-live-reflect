CXX      = g++
CPPFLAGS = -Wall -g -O2 -std=c++14 -DBOOST_LOG_DYN_LINK
TARGET   = srt-live-reflect
INCDIR   = -I./src -I/usr/local/include/srt
LIBDIR   = 
LIBS     = -lsrt -lpthread -lboost_thread -lboost_json -lboost_filesystem -lboost_log -lboost_chrono -lcurl
SRCDIR   = ./src
OBJDIR   = ./obj
BINDIR   = ./bin
SRCS     = $(wildcard $(SRCDIR)/*.cpp) 
OBJS     = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))


all: prepare $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(BINDIR)/$@ $^ $(LIBDIR) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(INCDIR) -c $< -o $@

prepare:
	mkdir -p $(BINDIR)
	mkdir -p $(OBJDIR)

clean:
	rm -f $(OBJDIR)/*.o
	rm -rf $(BINDIR)/*


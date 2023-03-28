CXX      = g++
CPPFLAGS = -Wall -g -O2 -std=c++14 -DBOOST_LOG_DYN_LINK
ifdef USE_AWSSDK
CPPFLAGS := $(CPPFLAGS) -DUSE_AWSSDK
endif
TARGET   = srt-live-reflect
INCDIR   = -I./src -I/usr/local/include/srt
LIBDIR   =
LIBS     = -lsrt -lpthread -lboost_thread -lboost_json -lboost_filesystem -lboost_log -lboost_chrono -lcurl
ifdef USE_AWSSDK
LIBS     := $(LIBS) /usr/local/lib64/libaws-cpp-sdk-s3.so
LIBS     := $(LIBS) /usr/local/lib64/libaws-cpp-sdk-core.so
LIBS     := $(LIBS) /usr/local/lib64/libaws-crt-cpp.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-mqtt.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-event-stream.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-s3.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-auth.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-http.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-io.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-compression.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-cal.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-sdkutils.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-checksums.a
LIBS     := $(LIBS) /usr/local/lib64/libaws-c-common.a
LIBS     := $(LIBS) /usr/local/lib64/libs2n.a
LIBS     := $(LIBS) /usr/lib64/libcrypto.so
endif
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

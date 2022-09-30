.SUFFIXES : .cpp .o

CC = gcc
AR = ar

INCDIR = ./inc
INCDIR1 = ./interface
SRCDIR = ./src
OUTDIR = ../out
MODDIR = $(OUTDIR)/Resampler
OBJDIR = $(MODDIR)/obj
LIBDIR = $(OUTDIR)/lib

CFLAGS = -g -Wall
ARFLAGS = rsc
TARGET = $(LIBDIR)/libresampler.a

SRCS  = $(wildcard $(SRCDIR)/*.cpp)
OBJS =  $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))

all: $(TARGET)

$(TARGET) : $(OBJS)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	@echo -e "\033[40;32mlibresampler.a Build Success!! \033[0m"

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(INCDIR1) -c $< -o $@

clean:
	rm -rf $(MODDIR)

CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-std=c++11
LDFLAGS=
LDLIBS=

SRCS=main.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: IrrigationServer

IrrigationServer: $(OBJS)
	    $(CXX) $(LDFLAGS) -o IrrigationServer $(OBJS) $(LDLIBS)

depend: .depend

.depend: $(SRCS)
	    $(RM) ./.depend
	    $(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	    $(RM) $(OBJS)

distclean: clean
	    $(RM) *~ .depend

include .depend

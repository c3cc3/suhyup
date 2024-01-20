include /ums/fq/src/Make.defines

SRCS = $(shell ls *.c)
OBJS = $(SRCS:.c=.o) #(patsubst %c,%o$(SRCS))

TARGET=GenLicense

all: dep $(OBJS)
	@echo ""
	@echo ""
	@echo "==========<start : Linking all objects and libraries >==========="
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -I$(FQ_INC) -L$(FQ_LIB) $(LIBS) -lcurses
	@echo "==========<end   : Linking all objects and libraries >==========="
	
clean:
	rm -f *.o *.so *.swp core $(TARGETS)

run:
	
#make dependencies
dep:$(SRCS)
	$(CC) -MM $(SRCS) -I $(FQ_INC) > .depend

#ruls
%.o:%.c
	$(CC) -c $< -I $(FQ_INC)
%.o:%.cpp
	$(CPP) -c $< -I $(FQ_INC)

#dependencies
-include .depend

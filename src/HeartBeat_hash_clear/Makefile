include /ums/fq/Make.defines

SRCS = $(shell ls *.c)
OBJS = $(SRCS:.c=.o) #(patsubst %c,%o$(SRCS))

TARGET=hash_clear
CONF_FILE=hash_clear.conf

all: dep $(OBJS)
	@echo ""
	@echo ""
	@echo "==========<start : Linking all objects and libraries >==========="
#	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -I$(FQ_INC) -L$(FQ_LIB) $(LIBS) -lcurses
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -I$(FQ_INC) -L$(FQ_LIB) $(LIBS)
	@echo "==========<end   : Linking all objects and libraries >==========="
	
clean:
	rm -f *.o *.so *.swp core $(TARGET)

run:
	./$(TARGET) ./$(CONF_FILE)

install:
	cp -f $(TARGET) $(BIN_INSTALL_PATH)

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

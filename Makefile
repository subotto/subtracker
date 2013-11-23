CFLAGS = `pkg-config --cflags opencv` -g
LIBS = `pkg-config --libs opencv`

% : %.cpp
	g++ $(CFLAGS) $(LIBS) -o $@ $<

all: track
	

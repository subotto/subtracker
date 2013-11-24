CFLAGS = `pkg-config --cflags opencv` -g -O2
LIBS = `pkg-config --libs opencv`

% : %.cpp
	g++ $(CFLAGS) $(LIBS) -o $@ $<

all: track

clean:
	rm -f track kalman

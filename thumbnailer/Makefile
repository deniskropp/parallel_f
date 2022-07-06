all: thumbnailer

thumbnailer: thumbnailer.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $< `pkg-config --cflags --libs sfml-graphics`

clean:
	rm -f thumbnailer

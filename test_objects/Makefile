all: test_objects

test_objects: test_objects.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $<

clean:
	rm -f test_objects

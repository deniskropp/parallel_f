all: test_pause

test_pause: test_pause.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $<

clean:
	rm -f test_pause

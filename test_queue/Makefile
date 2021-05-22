all: test_queue

test_queue: test_queue.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $<

clean:
	rm -f test_queue

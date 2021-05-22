all: test_flush_join

test_flush_join: test_flush_join.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $<

clean:
	rm -f test_flush_join

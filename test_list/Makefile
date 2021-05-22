all: test_list

test_list: test_list.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $<

clean:
	rm -f test_list

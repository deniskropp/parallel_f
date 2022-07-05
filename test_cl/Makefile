all: test_cl

test_cl: test_cl.cpp ../OCL_Device.cpp ../Util.cpp
	$(CXX) -pthread -std=c++17 -I.. -O2 -g2 -o $@ $+ -lOpenCL

clean:
	rm -f test_cl

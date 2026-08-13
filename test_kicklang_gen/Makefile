GENERATED = \
	generated_parallel_dag \
	generated_vision \
	generated_hello_queue \
	generated_speculative

WRAPPER = ../ocs-wrapper/parallel_f-kicklang-wrapper
EXAMPLES = $(WRAPPER)/examples
GEN = python3 $(WRAPPER)/generate_cpp.py

CXXFLAGS = -pthread -std=c++17 -I.. -O2 -g2

all: generate $(GENERATED)

generate:
	$(GEN) \
		$(EXAMPLES)/advanced_dependency_example.kl \
		$(EXAMPLES)/parallel_vision_example.kl \
		$(EXAMPLES)/hello_queue.kl \
		$(EXAMPLES)/speculative_fast_path.kl \
		--out-dir .

generated_parallel_dag: generated_parallel_dag.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

generated_vision: generated_vision.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

generated_hello_queue: generated_hello_queue.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

generated_speculative: generated_speculative.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: all
	@echo "=== hello_queue ==="
	@./generated_hello_queue
	@echo "=== advanced DAG ==="
	@./generated_parallel_dag
	@echo "=== vision ==="
	@./generated_vision
	@echo "=== speculative ==="
	@./generated_speculative

clean:
	rm -f $(GENERATED) generated_*.cpp

.PHONY: all generate run clean

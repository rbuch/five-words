OPTS ?= -Ofast -fopenmp -std=c++11

fiveletterwords : fiveletterwords.o
	$(CXX) $(OPTS) -o $@ $<

fiveletterwords.o : fiveletterwords.cpp
	$(CXX) $(OPTS) -c $<

.PHONY: clean
clean:
	$(RM) fiveletterwords fiveletterwords.o

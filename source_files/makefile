all:main.cc use_counting.cc
	g++ $^ -o webserver -std=c++20 -Wall -I ../header_files -lpthread -lzip -ljsoncpp
.PHONY:clean
clean:
	rm webserver -f

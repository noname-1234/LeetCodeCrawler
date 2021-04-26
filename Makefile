
.PHONY: all clean

all: crawler.cpp
	g++ crawler.cpp -lboost_system -lboost_filesystem -ljsoncpp -lcurl -o lc_crawler

install:
	install -m 0755 lc_crawler /usr/local/bin

clean:
	rm lc_crawler

compile:
	rm -f exchange.wast exchange.wasm
	eosiocpp -o exchange.wast exchange.cpp

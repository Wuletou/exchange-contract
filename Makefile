BUILD_DIR=build

compile:
	mv $(BUILD_DIR)/*.abi .
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)
	mv *.abi $(BUILD_DIR)
	eosiocpp -o $(BUILD_DIR)/exchange.wast exchange.cpp

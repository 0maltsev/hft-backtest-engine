build-debug:
	rm -rf build/debug build/release              
	cmake --preset debug
	cmake --build build/debug

build-release:
	rm -rf build/debug build/release                   
	cmake --preset release
	cmake --build build/release

run-debug:
	./build/debug/hft-backtest-engine   
run-release:
	./build/release/hft-backtest-engine
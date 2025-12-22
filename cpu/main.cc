#include <bits/stdc++.h>
#include "Benchmark.h"
using namespace std;

void comparison_test(string path, string save_path) {
	int start = 2000, end = 32000, step = 2;
	Benchmark benchmark(path, save_path);
	benchmark.generate_ground_truth();
	
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_KLL_Polymer(memory);
	}
	std::cout << "\n";
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_HistSketch(memory);
	}
	std::cout << "\n";
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_SQUAD(memory);
	}
	std::cout << "\n";
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_SketchPolymer(memory);
	}
	std::cout << "\n";
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_M4(memory);
	}
	std::cout << "\n";
	for (int memory = start; memory <= end; memory *= step) {
		benchmark.Run_KLL_Polymer_DC(memory);
	}
	std::cout << "\n";
}

void dc_test(string path, string save_path) {
	int start = 10, end = 30, step = 1;
	int memory = 8000;

	Benchmark** benchmark = new Benchmark* [21];
	for (int alpha = start; alpha <= end; alpha += step) {
		string new_path = path + std::to_string(alpha) + ".bin";
		benchmark[alpha - 10] = new Benchmark(new_path, save_path);
		benchmark[alpha - 10]->generate_ground_truth();
	}
	for (int alpha = start; alpha <= end; alpha += step) {
		benchmark[alpha - 10]->Run_KLL_Polymer(memory);
	}
	for (int alpha = start; alpha <= end; alpha += step) {
		benchmark[alpha - 10]->Run_KLL_Polymer_DC(memory);
	}
}



int main(int argc, char** argv) {
	string path = argv[1], save_path = argv[2];
	comparison_test(path, save_path);
	// dc_test(path, save_path);
	return 0;
}
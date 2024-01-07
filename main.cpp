#include <base/base.hpp>
#include <base/doctest.h>

namespace pb {

	static constexpr int VERSION_MAJOR = 0;
	static constexpr int VERSION_MINOR = 8;

};

int main(int argc, const char** argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'v' :
					printf("ver = %1i.%1i\n", pb::VERSION_MAJOR, pb::VERSION_MINOR);
				break;
				case 't' :
					printf("unit testing started up!\n");
					// unit-testing
					doctest::Context context(argc - i, argv + i);
					int res = context.run();
					if (context.shouldExit() || res) {
						printf("Test failed or user wants to stop\n");
						return res;
					}
				break;
			};
		}
	}
	return 0;
}

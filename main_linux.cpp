// Linux entrypoint stub. Phase 1: just exercises the build chain.
// CLI parsing + headless render arrive in Phase 2 with the GLFW window.

#include <cstdio>

int main(int /*argc*/, char ** /*argv*/) {
	std::fprintf(stderr,
		"Imagina Linux port -- Phase 1 build stub.\n"
		"Build succeeded.\n"
		"Headless CLI + GLFW window coming in Phase 2.\n");
	return 0;
}

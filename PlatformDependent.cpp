#include "PlatformDependent.h"

#ifdef _WIN32

#if __has_include(<Windows.h>)
#include <Windows.h>
#else
#include <windows.h>
#endif

void SetWorkerPriority() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

void ErrorMessage(const char *Message) {
	MessageBoxA(nullptr, Message, nullptr, MB_TASKMODAL | MB_OK);
}

void ErrorMessage(const char *Title, const char *Message) {
	MessageBoxA(nullptr, Message, Title, MB_TASKMODAL | MB_OK);
}

#else

#include <cstdio>
#include <pthread.h>
#include <sched.h>

void SetWorkerPriority() {
	struct sched_param p{};
	p.sched_priority = 0;
	pthread_setschedparam(pthread_self(), SCHED_IDLE, &p);
}

void ErrorMessage(const char *Message) {
	std::fprintf(stderr, "Imagina: %s\n", Message ? Message : "");
}

void ErrorMessage(const char *Title, const char *Message) {
	std::fprintf(stderr, "Imagina [%s]: %s\n",
		Title ? Title : "", Message ? Message : "");
}

#endif

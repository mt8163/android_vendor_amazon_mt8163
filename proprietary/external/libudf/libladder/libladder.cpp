#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
//#include <atomic>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include <android-base/stringprintf.h>
#include <unwindstack/Log.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>
#include <libladder.h>
#include <log/log.h>
#include <demangle.h>
#include <inttypes.h>
#include <time.h>



#define THREAD_SIGNAL (__SIGRTMIN+1)


namespace unwindstack {

///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

static std::atomic_bool g_finish;
static std::atomic_uintptr_t g_ucontext;
static pthread_mutex_t g_process_unwind_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sigaction_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wait_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_wait_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char* PATH_THREAD_NAME = "/proc/self/task/%d/comm";
static std::string getThreadName(pid_t tid) {
	char path[PATH_MAX];
	char* procName = NULL;
	char procNameBuf[128];
	FILE* fp;
	snprintf(path, sizeof(path), PATH_THREAD_NAME, tid);
	if ((fp = fopen(path, "r"))) {
		procName = fgets(procNameBuf, sizeof(procNameBuf), fp);
		fclose(fp);
	} else {
		ALOGE("%s: Failed to open %s", __FUNCTION__, path);
        }
	if (procName == NULL) {
		// Reading /proc/self/task/%d/comm failed due to a race
		return android::base::StringPrintf("[err-unknown-tid-%d]", tid);
	}
	// Strip ending newline
	strtok(procName, "\n");
	return android::base::StringPrintf("%s", procName);
}


static void GetThreads(pid_t pid, std::vector<pid_t>* threads) {
	// Get the list of tasks.
	char task_path[128];
	snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);

	std::unique_ptr<DIR, decltype(&closedir)> tasks_dir(opendir(task_path), closedir);
	//ASSERT_TRUE(tasks_dir != nullptr);
	struct dirent* entry;
	while ((entry = readdir(tasks_dir.get())) != nullptr) {
		char* end;
		pid_t tid = strtoul(entry->d_name, &end, 10);
		if (*end == '\0') {
		threads->push_back(tid);
		}
 	}
}


static void SignalHandler(int, siginfo_t*, void* sigcontext) {
	g_ucontext = reinterpret_cast<uintptr_t>(sigcontext);
	pthread_mutex_lock(&g_wait_mutex);
	pthread_cond_signal(&g_wait_cond);
	pthread_mutex_unlock(&g_wait_mutex);
	while (!g_finish.load()) {
	}
}

static std::string FormatFrame(const FrameData& frame) {
	std::string data;

	if(frame.pc < (uint64_t)0xffffffff) {
		data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, frame.num, frame.rel_pc);
	} else {
		data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, frame.num, frame.rel_pc);
	}

	if (frame.map_offset != 0) {
		data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")", frame.map_offset);
	}

	if (frame.map_start == frame.map_end) {
		// No valid map associated with this frame.
		data += "  <unknown>";
	} else if (!frame.map_name.empty()) {
		data += "  " + frame.map_name;
	} else {
		data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">", frame.map_start);
	}

	if (!frame.function_name.empty()) {
		data += " (" + demangle(frame.function_name.c_str());
		if (frame.function_offset != 0) {
			data += android::base::StringPrintf("+%" PRId64, frame.function_offset);
		}
		data += ')';
	}
	return data;
}

static bool UnwindThread(pid_t tid, std::string *str) {
	struct sigaction act, oldact;
	pid_t pid= getpid();
	g_finish = false;
	g_ucontext = 0;
	pthread_mutex_lock(&g_sigaction_mutex);
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = SignalHandler;
	act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
	if(sigaction(THREAD_SIGNAL, &act, &oldact) !=0) {
		ALOGE("sigaction failed: %s", strerror(errno));
		pthread_mutex_unlock(&g_sigaction_mutex);
		return false;
    	}
	ALOGE("unwind pid(%d),tid(%d)\n",pid,tid);
	// Portable tgkill method.
	if (tgkill(pid, tid, THREAD_SIGNAL) != 0) {
		// Do not emit an error message, this might be expected. Set the
		// error and let the caller decide.
		if (errno == ESRCH) {
			ALOGE("errno == ESRCH: %s,tid:%d", strerror(errno),tid);//BACKTRACE_UNWIND_ERROR_THREAD_DOESNT_EXIST;
		} else {
			ALOGE("errno != ESRCH: %s,tid:%d", strerror(errno),tid);
		}
	}else {
		// Wait for context data.
		void* ucontext;
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			ALOGE("Get clock time error");
		}
		ts.tv_sec += 5;
		pthread_mutex_lock(&g_wait_mutex);
		int ret = pthread_cond_timedwait(&g_wait_cond, &g_wait_mutex, &ts);
		if (ret != 0) {
			ALOGE("pthread_cond_timedwait failed:ret %d, %s", ret,strerror(ret));
			g_ucontext = 0; //in order to exit in below "if(ucontext == nullptr)"
		}
		ucontext = reinterpret_cast<void*>(g_ucontext.load());
		pthread_mutex_unlock(&g_wait_mutex);

		if(ucontext == nullptr){
			ALOGE("context == nullptr, then break\n");
			sigaction(THREAD_SIGNAL, &oldact, nullptr);
			g_finish = true;
			pthread_mutex_unlock(&g_sigaction_mutex);
			return false;
		}
		LocalMaps maps;  ///   proc/pid/maps"
		if(maps.Parse()==false) {
			ALOGE("UnwindCurThreadBT,parse maps fail\n");
			sigaction(THREAD_SIGNAL, &oldact, nullptr);
			g_finish = true;
			pthread_mutex_unlock(&g_sigaction_mutex);
			return false;
		}

		std::unique_ptr<Regs> regs(Regs::CreateFromUcontext(Regs::CurrentArch(), ucontext));
		// VerifyUnwind(getpid(), &maps, regs.get(), kFunctionOrder);
		auto process_memory(Memory::CreateProcessMemory(getpid()));

		Unwinder unwinder(512, &maps, regs.get(), process_memory);
		unwinder.Unwind();

		// Print the frames.
		*str += android::base::StringPrintf(" pid(%d) tid(%d) ", pid,tid);
		*str += getThreadName(tid) + "\n";

		//ALOGV("unwind pid(%d),tid(%d)\n",pid,tid);
		for (size_t i = 0; i < unwinder.NumFrames(); i++) {
			struct FrameData frame = unwinder.frames()[i];
			*str += FormatFrame(frame) + "\n";
			//ALOGE("%s\n",demangle(unwinder.FormatFrame(i).c_str()).c_str() );
		}
	}
	sigaction(THREAD_SIGNAL, &oldact, nullptr);
	g_finish = true;
	pthread_mutex_unlock(&g_sigaction_mutex);
	//ALOGV("unwind tid(%d),done\n",tid);

	return true;
}

extern "C" bool UnwindCurProcessBT(std::string *strBacktrace);

bool UnwindCurProcessBT(std::string *strBacktrace) {
	bool ret=false;
	pid_t pid=getpid();
	pid_t tid=gettid();
	std::vector<pid_t> threads;
	GetThreads(pid, &threads);
	strBacktrace->clear();

	/*lock successfully when pthread_mutex_trylock return 0.*/
	if (pthread_mutex_trylock(&g_process_unwind_mutex))
		return false;
	for (std::vector<int>::const_iterator it = threads.begin(); it != threads.end(); ++it)
	{
		// Skip the current forked process, we only care about the threads.
		if (tid == *it) {
			//ALOGE("unwind the current thread tid(%x)\n",tid);

			LocalMaps maps;  ///   proc/pid/maps"
			if(maps.Parse()==false) {
				ALOGE("UnwindCurThreadBT,parse maps fail\n");
				pthread_mutex_unlock(&g_process_unwind_mutex);
				return false;
			}

			std::unique_ptr<Regs> regs(Regs::CreateFromLocal());
			RegsGetLocal(regs.get());

			auto process_memory(Memory::CreateProcessMemory(getpid()));
			Unwinder unwinder(512, &maps, regs.get(), process_memory);
			unwinder.Unwind();

			// Print the frames.
			*strBacktrace += android::base::StringPrintf(" pid(%d) tid(%d) ", pid,tid);
			*strBacktrace += getThreadName(tid) + "\n";
			for (size_t i = 0; i < unwinder.NumFrames(); i++) {
				struct FrameData frame = unwinder.frames()[i];
				*strBacktrace += FormatFrame(frame) + "\n";
			}
			continue;
		}
		ret=UnwindThread(*it,strBacktrace);
	}
	ALOGI("UnwindCurProcessBT +++\n");
	pthread_mutex_unlock(&g_process_unwind_mutex);
	return ret;
}

extern "C" bool UnwindCurThreadBT(std::string *strBacktrace);
bool UnwindCurThreadBT(std::string *strBacktrace) {
	LocalMaps maps;  ///   proc/pid/maps"
	if(maps.Parse()==false) {
		ALOGE("UnwindCurThreadBT,parse maps fail\n");
		return false;
	}

	std::unique_ptr<Regs> regs(Regs::CreateFromLocal());
	RegsGetLocal(regs.get());

	auto process_memory(Memory::CreateProcessMemory(getpid()));
	Unwinder unwinder(512, &maps, regs.get(), process_memory);
	unwinder.Unwind();

	// Print the frames.
	pid_t pid=getpid();
	pid_t tid=gettid();
	strBacktrace->clear();
	ALOGI("unwind the current thread tid(%d)\n",tid);
	*strBacktrace += android::base::StringPrintf(" pid(%d) tid(%d) ", pid,tid);
	*strBacktrace += getThreadName(tid) + "\n";
	for (size_t i = 0; i < unwinder.NumFrames(); i++) {
		struct FrameData frame = unwinder.frames()[i];
		*strBacktrace += FormatFrame(frame) + "\n";
	}
	return true;
}


extern "C" bool UnwindThreadBT(pid_t tid,std::string *strBacktrace);
bool UnwindThreadBT(pid_t tid,std::string *strBacktrace) {
	pid_t pid=getpid();
	pid_t Curtid=gettid();
	strBacktrace->clear();
	bool ret=false;
	if(tid == Curtid)
		return UnwindCurThreadBT(strBacktrace);
	else {
		std::vector<pid_t> threads;
		GetThreads(pid, &threads);
		for (std::vector<int>::const_iterator it = threads.begin(); it != threads.end(); ++it)
		{
			if (tid == *it) {
				return UnwindThread(*it,strBacktrace);
			}
		}
	}
	return ret;
}


extern "C" bool UnwindCurProcessBT_Vector(std::vector<std::string> *strBacktrace);

bool UnwindCurProcessBT_Vector(std::vector<std::string> *strBacktrace) {
	bool ret=false;
	pid_t pid=getpid();
	pid_t tid=gettid();
	std::vector<pid_t> threads;
	GetThreads(pid, &threads);
	strBacktrace->clear();

	/*lock successfully when pthread_mutex_trylock return 0.*/
	if (pthread_mutex_trylock(&g_process_unwind_mutex))
		return false;
	for (std::vector<int>::const_iterator it = threads.begin(); it != threads.end(); ++it)
	{
		// Skip the current forked process, we only care about the threads.
		if (tid == *it) {
			//ALOGE("unwind the current thread tid(%x)\n",tid);

			LocalMaps maps;  ///   proc/pid/maps"
			if(maps.Parse()==false) {
				ALOGE("UnwindCurProcessBT_Vector,parse maps fail\n");
				pthread_mutex_unlock(&g_process_unwind_mutex);
				return false;
			}

			std::unique_ptr<Regs> regs(Regs::CreateFromLocal());
			RegsGetLocal(regs.get());

			auto process_memory(Memory::CreateProcessMemory(getpid()));
			Unwinder unwinder(512, &maps, regs.get(), process_memory);
			unwinder.Unwind();

			// Print the frames.
			std::string strThreadBacktrace;
			strThreadBacktrace += android::base::StringPrintf(" pid(%d) tid(%d) ", pid,tid);
			strThreadBacktrace += getThreadName(tid) + "\n";
			for (size_t i = 0; i < unwinder.NumFrames(); i++) {
				struct FrameData frame = unwinder.frames()[i];
				strThreadBacktrace += FormatFrame(frame) + "\n";
			}
			(*strBacktrace).push_back(strThreadBacktrace);
			continue;
		}
		else
		{
			std::string strThreadBacktrace;
			if(UnwindThread(*it,&strThreadBacktrace))
				(*strBacktrace).push_back(strThreadBacktrace);
		}
	}
	ALOGI("UnwindCurProcessBT_Vector +++\n");
	pthread_mutex_unlock(&g_process_unwind_mutex);
	return ret;
}


}

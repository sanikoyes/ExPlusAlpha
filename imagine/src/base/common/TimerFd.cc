#include <base/Base.hh>
#include <util/collection/DLList.hh>

#if defined CONFIG_BASE_ANDROID
	// No sys/timerfd.h on Android, need to use syscall
	#include <time.h>
	#include <sys/syscall.h>

	static int timerfd_create(clockid_t __clock_id, int __flags)
	{
		return syscall(__NR_timerfd_create, __clock_id, __flags);
	}

	static int timerfd_settime(int __ufd, int __flags,
						const struct itimerspec *__utmr,
						struct itimerspec *__otmr)
	{
		return syscall(__NR_timerfd_settime, __ufd, __flags, __utmr, __otmr);
	}
#else
	#include <sys/timerfd.h>
#endif

namespace Base
{

class TimerFdHandler;
static void onTimerComplete(TimerFdHandler *timer);

class TimerFdHandler
{
public:
	constexpr TimerFdHandler() { }

	void setCallback(CallbackDelegate callback)
	{
		var_selfs(callback);
	}

	bool armMs(int ms)
	{
		if(fd == -1)
		{
			fd = timerfd_create(CLOCK_MONOTONIC, 0);
			if(fd == -1)
			{
				logErr("error creating timerfd");
				return 0;
			}
			addPollEvent(fd, pollEvDel, Base::POLLEV_IN);
		}

		int seconds = ms / 1000;
		int leftoverMs = ms % 1000;
		long leftoverNs = leftoverMs * 1000000;
		logMsg("setting timerfd to run in %d second(s) and %ld ns", seconds, leftoverNs);
		struct itimerspec newTime { { 0 }, { seconds, leftoverNs } }, oldTime;
		if(timerfd_settime(fd, 0, &newTime, &oldTime) != 0)
		{
			logErr("error in timerfd_settime");
			return 0;
		}
		return 1;
	}

	int runCallback(int events)
	{
		logMsg("running callback, fd %d", fd);
		callback.invoke();
		onTimerComplete(this);
		return 1;
	}

	void cancel()
	{
		if(fd >= 0)
		{
			struct itimerspec newTime { { 0 } }, oldTime;
			timerfd_settime(fd, 0, &newTime, &oldTime);
		}
	}

	void deinit()
	{
		if(fd >= 0)
		{
			removePollEvent(fd);
			close(fd);
			fd = -1;
		}
	}

	bool operator ==(TimerFdHandler const& rhs) const
	{
		return callback == rhs.callback;
	}

private:
	Base::PollEventDelegate pollEvDel {Base::PollEventDelegate::create<TimerFdHandler, &TimerFdHandler::runCallback>(this)};
	CallbackDelegate callback;
	int fd = -1;
};

StaticDLList<TimerFdHandler, 4> timerList;

void onTimerComplete(TimerFdHandler *timer)
{
	timer->deinit();
	timerList.remove(*timer);
}

void cancelCallback(CallbackRef *ref)
{
	if(ref)
	{
		logMsg("removing callback");
		auto timer = ((TimerFdHandler*)ref);
		timer->deinit();
		timerList.remove(*timer);
	}
}

CallbackRef *callbackAfterDelay(CallbackDelegate callback, int ms)
{
	if(timerList.isFull())
	{
		bug_exit("using too many timers");
		return nullptr;
	}
	timerList.add();
	auto timer = timerList.first();
	timer->setCallback(callback);
	if(!timer->armMs(ms))
	{
		bug_exit("failed to setup timer, OS resources may be low or bad parameters present");
	}
	return (CallbackRef*)timer;
}

}

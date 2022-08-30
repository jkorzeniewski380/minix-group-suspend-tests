#include <cassert>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <lib.h>
#include <minix/rs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

// if pair.first is true, run pair.second on root process, otherwise create new one and run pair.second on it
std::vector<int> get_order(std::vector<std::pair<bool, std::function<void ()>>> thread_functions) {
	size_t n = 0;
    for (auto& data: thread_functions) {
        if (!data.first) {
            n++;
        }
    }

	std::vector<int> pids(n);
    size_t pid_idx = -1;
	for (size_t i = 0; i < thread_functions.size(); ++i) {
        if (thread_functions[i].first) {
            (thread_functions[i].second)();
            continue;
        }
        pid_idx++;
		pids[pid_idx] = fork();
		assert(pids[pid_idx] >= 0);
		if (pids[pid_idx] == 0) {
			(thread_functions[i].second)();
			_exit(0);
		}
	}
	int killer_pid = fork();
	if (killer_pid == 0) {
		sleep(1);
		for (size_t i = 0; i < n; ++i)
			kill(pids[i], SIGTERM);
		_exit(0);
	}

	std::vector<int> order;
	for (int i = 0; i < n + 1; ++i) {
		int wstatus;
		int child_pid = wait(&wstatus);
		assert(child_pid != -1);
		if (child_pid == killer_pid)
			continue;

		for (int j = 0; j < n; ++j)
			if (child_pid == pids[j])
				order.emplace_back(j);
	}
	assert(order.size() == n);

    std::cout << "Order: [";
    for (auto idx: order) {
        std::cout << idx << " ";
    }
    std::cout << "]\n";
    return order;
}

void sleep_units(int cnt) {
	std::this_thread::sleep_for(std::chrono::milliseconds(100 * cnt));
}

std::function<void ()> sleep_check_suspend(int sleep_pre_check, int sleep_post_check) {
    return [=] {
        sleep_units(sleep_pre_check);
        group_check();
        sleep_units(sleep_post_check);
    };
}

std::function<void ()> just_sleep(int sleep_before) {
    return [=] {
        sleep_units(sleep_before);
    };
}

std::function<void ()> stop_group(int sleep_before_stop) {
    return [=] {
        sleep_units(sleep_before_stop);
        group_stop();
    };
}

std::function<void ()> restart_group(int sleep_before_restart) {
    return [=] {
        sleep_units(sleep_before_restart);
        group_start();
    };
}

void test_simple() {
    assert(
        get_order({
            std::make_pair(true, stop_group(0)),
            std::make_pair(false, sleep_check_suspend(0, 1)),
            std::make_pair(true, restart_group(2)),
            std::make_pair(false, sleep_check_suspend(0, 1)),
            std::make_pair(false, just_sleep(2))
        }) == (std::vector<int>{0, 1, 2})
    );
}

void test_simple2() {
    assert(
            get_order({
                std::make_pair(true, stop_group(0)),
                std::make_pair(false, sleep_check_suspend(0, 3)),
                std::make_pair(true, restart_group(2)),
                std::make_pair(false, sleep_check_suspend(0, 0)),
                std::make_pair(false, just_sleep(4))
            }) == (std::vector<int>{1, 0, 2})
    );
}

void test_no_stop() {
    assert(
        get_order({
              std::make_pair(false, sleep_check_suspend(0, 5)),
              std::make_pair(false, sleep_check_suspend(0, 1)),
              std::make_pair(false, sleep_check_suspend(0, 3))
        }) == (std::vector<int>{1, 2, 0})
    );
}

void test_stop_after_restart() {
    assert(
        get_order({
              std::make_pair(true, stop_group(0)),
              std::make_pair(false, sleep_check_suspend(6, 3)), // won't pass check after first group_start
              std::make_pair(false, sleep_check_suspend(2, 2)), // will pass check after first group_start
              std::make_pair(true, restart_group(1)), // group_start
              std::make_pair(true, stop_group(3)), // wait 3 seconds then group_stop
              std::make_pair(false, sleep_check_suspend(0, 1)), // passess check immediately
              std::make_pair(true, restart_group(4)) // wait 4 seconds then group_start
        }) == (std::vector<int>{1, 2, 0})
    );
}

int main() {
    test_simple();
    test_simple2();
    test_no_stop();
    test_stop_after_restart();
}

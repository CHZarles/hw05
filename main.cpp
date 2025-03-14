// 小彭老师作业05：假装是多线程 HTTP 服务器 - 富连网大厂面试官觉得很赞
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>

struct User {
  std::string password;
  std::string school;
  std::string phone;
};

using double_ms = std::chrono::duration<double, std::milli>;
std::shared_mutex users_mtx;
std::map<std::string, User> users;
std::map<std::string, double_ms> has_login; // 换成 std::chrono::seconds 之类的

// 作业要求1：把这些函数变成多线程安全的
// 提示：能正确利用 shared_mutex 加分，用 lock_guard 系列加分
std::string do_register(std::string username, std::string password,
                        std::string school, std::string phone) {
  User user = {password, school, phone};
  // 这个地方是写数据
  std::lock_guard grd(users_mtx);
  if (users.emplace(username, user).second)
    return "注册成功";
  else
    return "用户名已被注册";
}

std::string do_login(std::string username, std::string password) {
  // 作业要求2：把这个登录计时器改成基于 chrono 的
  auto tik = std::chrono::steady_clock::now();
  if (has_login.find(username) != has_login.end()) {
    double ms = has_login.at(username).count();
    return std::to_string(ms) + "ms 内登录过";
  }
  auto tok = std::chrono::steady_clock::now();
  has_login[username] = std::chrono::duration_cast<double_ms>(tok - tik);

  // 这里要加锁吗 ？？？？
  std::shared_lock lock(users_mtx);
  if (users.find(username) == users.end())
    return "用户名错误";
  if (users.at(username).password != password)
    return "密码错误";
  return "登录成功";
}

std::string do_queryuser(std::string username) {
  auto &user = users.at(username);
  std::stringstream ss;
  ss << "用户名: " << username << std::endl;
  ss << "学校:" << user.school << std::endl;
  ss << "电话: " << user.phone << std::endl;
  return ss.str();
}

struct ThreadPool {
  std::queue<std::future<void>> f_queue;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<int> stop = 0;
  std::thread consumer;

  ThreadPool() {
    // start consumer
    consumer = std::thread([this] {
      while (stop.load() == 0 || !this->f_queue.empty()) {
        std::unique_lock grd(mtx);
        cv.wait(grd, [this] { return !this->f_queue.empty(); });
        f_queue.front().wait();
        f_queue.pop();
      }
    });
  }
  void create(std::function<void()> start) {
    if (stop.load() == 1) {
      std::cout << "threadpool stoped\n";
      return;
    }
    // 作业要求3：如何让这个线程保持在后台执行不要退出？
    // 提示：改成 async 和 future 且用法正确也可以加分
    std::unique_lock grd(mtx);
    f_queue.push(std::async(std::move(start)));
    grd.unlock();
    if (f_queue.size() == 1) {
      cv.notify_one();
    }
  }

  void stop_and_wait_all() {
    stop.store(1);
    consumer.join();
  }
};

ThreadPool tpool;

namespace test { // 测试用例？出水用力！
std::string username[] = {"张心欣", "王鑫磊", "彭于斌", "胡原名"};
std::string password[] = {"hellojob", "anti-job42", "cihou233", "reCihou_!"};
std::string school[] = {"九百八十五大鞋", "浙江大鞋", "剑桥大鞋",
                        "麻绳理工鞋院"};
std::string phone[] = {"110", "119", "120", "12315"};
} // namespace test

int main() {
  for (int i = 0; i < 262144; i++) {
    tpool.create([&] {
      std::cout << do_register(
                       test::username[rand() % 4], test::password[rand() % 4],
                       test::school[rand() % 4], test::phone[rand() % 4])
                << std::endl;
    });
    tpool.create([&] {
      std::cout << do_login(test::username[rand() % 4],
                            test::password[rand() % 4])
                << std::endl;
    });
    tpool.create([&] {
      std::cout << do_queryuser(test::username[rand() % 4]) << std::endl;
    });
    std::cout << "loop : " << i << "\n";
  }
  // 作业要求4：等待 tpool 中所有线程都结束后再退出
  tpool.stop_and_wait_all();
  return 0;
}

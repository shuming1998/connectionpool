#include <unistd.h>

#include "Public.h"
#include "ConnectionPool.h"

// 线程安全的懒汉单例
ConnectionPool *ConnectionPool::getConnectionPool() {
  // 对静态局部变量的初始化由编译器 lock unlock，且只有运行到此时才初始化(懒汉)
  static ConnectionPool poll;
  return &poll;
}

shared_ptr<Connection> ConnectionPool::getConnection() {
  unique_lock<mutex> lock(queueMutex_);
  while (connQueue_.empty()) {
    // 最多等待 connectionTimeout_ 毫秒，而非直接 sleep 指定时间
    if (cv_status::timeout == cv_.wait_for(lock, chrono::milliseconds(connectionTimeout_))) {
      // 因超时而醒来后，发现队列仍为空
      if (connQueue_.empty()) {
        LOG("Timeout! Get free connection failed!");
        return nullptr;
      }
    }
  }
  // shared_ptr 智能指针析构时会把资源 delete，conn 就会被 close 而这里需要将连接归还给消费队列
  // 所以需要自定义 shared_ptr 释放资源的方式(shared_ptr 第二个参数是一个释放器函数，这里用 lambda 即可)
  shared_ptr<Connection> sp(connQueue_.front(), [&](Connection *pconn){
    // 这里是在服务器应用线程中调用，所以必须考虑对队列的线程安全操作
    unique_lock<mutex> lock(queueMutex_);
    // 刷新连接进入队列的时间
    pconn->refreshAliveTime();
    connQueue_.push(pconn);
  });
  connQueue_.pop();
  // 当前线程消费完连接之后通知生产者线程，生产者线程会判断是否需要生产新连接
  cv_.notify_all();
  return sp;
}

ConnectionPool::ConnectionPool() {
  // 加载配置项
  if (!loadConfigFile()) {
    return;
  }
  // 创建初始数量的连接
  for (int i = 0; i < initSize_; ++i) {
    Connection *p = new Connection();
    p->connect(ip_, port_, userName_, passWord_, dbName_);
    // 记录连接进入队列的时间
    p->refreshAliveTime();
    connQueue_.push(p);
    ++connCnts_;
  }
  // 启动新线程作为连接的生产者
  // 成员方法作线程函数，需要绑定 this 指针(线程函数都是 c 接口)
  thread producer(std::bind(&ConnectionPool::produceConnTask, this));
  producer.detach();

  // 启动新的定时线程，回收空闲时间超过 maxIdleTime 的多余连接
  thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
  scanner.detach();
}

// 当连接队列为空且未超过最大连接量时，生产新连接
void ConnectionPool::produceConnTask() {
  for (;;) {
    // 生产者线程加锁，消费者线程无法获取锁
    unique_lock<mutex> lock(queueMutex_);
    while (!connQueue_.empty()) {
      // 连接队列非空时，生产线程进入等待状态并释放锁，消费者线程可获取锁
      cv_.wait(lock);
    }

    // 连接队列为空 且连接最大数未超过最大限制时，生产新连接
    if (connCnts_ < maxSize_) {
      Connection *p = new Connection();
      p->connect(ip_, port_, userName_, passWord_, dbName_);
      // 记录连接进入队列的时间
      p->refreshAliveTime();
      connQueue_.push(p);
      ++connCnts_;
    }

    // 通知消费者线程可以从连接队列中消费连接
    cv_.notify_all();
  }
  // 出作用域，释放锁
}

void ConnectionPool::scannerConnectionTask() {
  for (;;) {
    // 通过 sleep 模拟定时
    this_thread::sleep_for(chrono::seconds(maxIdleTime_));

    // 扫描连接队列，释放多余的连接
    unique_lock<mutex> lock(queueMutex_);
    while (connCnts_ > initSize_) {
      Connection *pconn = connQueue_.front();
      // 队首连接的存活时间 >= 最大存活时间，释放连接
      if (pconn->getAliveTime() >= (maxIdleTime_ * 1000)) {
        connQueue_.pop();
        --connCnts_;
        delete pconn;
        // 队首连接的存活时间 < 最大存活时间，其余连接就不用考虑
      } else {
        break;
      }
    }
  }
}

bool ConnectionPool::loadConfigFile() {
  char buf[1024] = {0};
  string filePath = string(getcwd(buf, 1024)) + string("/mysql.conf");
  FILE *pf = fopen(filePath.c_str(), "r");
  if (pf == nullptr) {
    LOG("mysql.conf doesn't exist!");
    return false;
  }

  while (!feof(pf)) {
    char line[1024] = {0};
    fgets(line, 1024, pf);
    string str = line;
    int idx = str.find('=', 0);
    // 注释或配置无效
    if (-1 == idx) {
      continue;
    }
    int endIndex = str.find('\n', idx);
    string key = str.substr(0, idx);
    string value = str.substr(idx + 1, endIndex - idx - 1);
    if (key == "ip") {
      ip_ = value;
    } else if (key == "port") {
      port_ = atoi(value.c_str());
    } else if (key == "username") {
      userName_ = value;
    } else if (key == "password") {
      passWord_ = value;
    } else if (key == "dbname") {
      dbName_ = value;
    } else if (key == "initsize") {
      initSize_ = atoi(value.c_str());
    } else if (key == "maxSize") {
      maxSize_ = atoi(value.c_str());
    } else if (key == "maxIdleTime") {
      maxIdleTime_ = atoi(value.c_str());
    } else if (key == "connectionTimeout") {
      connectionTimeout_ = atoi(value.c_str());
    }
  }
  return true;
}
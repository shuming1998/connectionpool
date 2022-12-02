#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <iostream>
#include <functional>
#include <condition_variable>

#include "Connection.h"
using namespace std;

// 数据库连接池
class ConnectionPool{
public:
  static ConnectionPool *getConnectionPool();
  // 给外部提供接口，从连接池中获取一个可用的空闲连接的智能指针，不需要再定义归还接口
  shared_ptr<Connection> getConnection();

private:
  // 单例构造
  ConnectionPool();
  // 加载配置项
  bool loadConfigFile();
  // 运行在独立的线程中，用于生产新连接
  void produceConnTask();
  // 扫描多余的空闲连接，将空闲时间超过 maxIdleTime_的连接释放
  void scannerConnectionTask();

  string ip_;                 // MySQL ip 地址
  unsigned short port_;       // MySQL 端口号
  string userName_;           // MySQL 登录用户名
  string passWord_;           // MySQL 登录密码
  string dbName_;             // 连接的数据库名称
  int initSize_;              // 连接池初始连接量
  int maxSize_;               // 连接池最大连接量
  int maxIdleTime_;           // 连接池最大空闲时间
  int connectionTimeout_;     // 连接池获取连接的超时时间

  queue<Connection *> connQueue_; // MySQL 连接队列
  mutex queueMutex_;              // 连接队列互斥锁
  atomic_int connCnts_;           // 记录创建的连接总数
  condition_variable cv_;          // 条件变量，用于连接生产线程和连接消费线程之间通信
};


#endif
#ifndef CONNECTION_H
#define CONNECTION_H

#include <mysql/mysql.h>
#include <string>
#include <ctime>
using namespace std;

class Connection {
public:
  // 开辟连接所需内存空间
  Connection();
  // 释放数据库连接资源
  ~Connection();
  // 连接数据库
  bool connect(string ip, unsigned short port, string user, string password, string dbname);
  // 更新操作
  bool update(string sql);
  // 查询操作
  MYSQL_RES *query(string sql);
  // 刷新进入连接队列时的时间
  void refreshAliveTime();
  // 返回在连接队列中存活的时间
  clock_t getAliveTime() const;

private:
  MYSQL *conn_;
  clock_t aliveTime_; // 进入连接队列时的时间
};

#endif